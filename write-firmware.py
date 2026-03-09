#!/usr/bin/env python3
"""CANopen OTA test for AT32 kernel + PLC.

Performs an update via CanOpen:
- 0x5F00 — fwUpdateControl
- 0x5F01 — fwUpdateStatus
- 0x5F02 — fwUpdateData
- 0x5F03 — fwUpdateInfo
"""

from __future__ import annotations

import argparse
import binascii
import glob
import struct
import time
from dataclasses import dataclass
from pathlib import Path
from random import randrange
from typing import Optional
import serial.tools.list_ports

try:
    import serial
except ImportError as exc:
    raise SystemExit("pyserial is required: pip install pyserial\n" + str(exc))


OTA_CMD_START = 0x01
OTA_CMD_END = 0x03
OTA_CMD_ABORT = 0x04
OTA_CMD_STATUS_REQ = 0x05
OTA_CMD_APPLY = 0x06

OTA_TARGET_AT32 = 0x00
OTA_TARGET_ASR = 0x01

OTA_STATE_IDLE = 0x00
OTA_STATE_RECV = 0x01
OTA_STATE_VERIFY = 0x02
OTA_STATE_FLASH = 0x03
OTA_STATE_REBOOT = 0x04
OTA_STATE_ERROR = 0x05

OTA_IMAGE_TYPE_KERNEL = 0x00
OTA_IMAGE_TYPE_PLC = 0x01

SDO_DOWNLOAD_1BYTE = 0x2F
SDO_DOWNLOAD_2BYTE = 0x2B
SDO_DOWNLOAD_4BYTE = 0x23
SDO_DOWNLOAD_INIT_SEGMENTED = 0x21
SDO_DOWNLOAD_SEGMENT = 0x00
SDO_UPLOAD_REQUEST = 0x40
SDO_UPLOAD_REPLY_1BYTE = 0x4F
SDO_UPLOAD_REPLY_2BYTE = 0x4B
SDO_UPLOAD_REPLY_4BYTE = 0x43
SDO_ABORT = 0x80
SDO_SEGMENT_DOWNLOAD_RESPONSE_MASK = 0xE0
SDO_SEGMENT_RESPONSE_EXPECT = 0x20
SDO_ABORT_TOGGLE_NOT_ALTERNATED = 0x05030000
SDO_ABORT_TIMEOUT = 0x05040000
SDO_ABORT_CMD_SPEC_UNKNOWN = 0x05040001

OTA_CONTROL_OBJECT = 0x5F00
PROJECT_ROOT = Path(__file__).resolve().parents[0]
DEFAULT_PROFILE = "firmware"
DEFAULT_PROFILE_BY_PRODUCT = {
    0x00000000: "firmware",
}
DEFAULT_PLC_BY_PROFILE = {
    "firmware": None,
}

CAN_ID_SDO_REQUEST = 0x600
CAN_ID_SDO_RESPONSE = 0x580

@dataclass(frozen=True)
class CanFrame:
    can_id: int
    data: bytes
    timestamp: float

@dataclass(frozen=True)
class OtaStatus:
    state: int
    last_error: int
    bytes_received: int
    bytes_written: int
    image_crc32: int
    progress: int

class SdoAbortError(RuntimeError):
    def __init__(self, index: int, sub: int, code: int, *, recoverable: bool = False) -> None:
        self.index = index
        self.sub = sub
        self.code = code
        self.recoverable = recoverable
        super().__init__(
            f"SDO abort: 0x{index:04X}:0x{sub:02X} code=0x{code:08X}"
            + (" (recoverable)" if recoverable else "")
        )

class UsbCanBus:
    def __init__(self, port: str, baudrate: int = 115200, timeout: float = 0.2) -> None:
        self.serial = serial.Serial(
            port=port,
            baudrate=baudrate,
            timeout=timeout,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            rtscts=False,
            dsrdtr=False,
        )

    def send(self, payload: str) -> None:
        if not payload.endswith("\r"):
            payload += "\r"
        self.serial.write(payload.encode("ascii"))
        self.serial.flush()

    def send_frame(self, can_id: int, data: bytes) -> None:
        if not (0 <= can_id <= 0x7FF):
            raise ValueError("Unsupported CAN ID")
        if not (0 <= len(data) <= 8):
            raise ValueError("CAN payload length must be 0..8")
        self.send(f"t{can_id:03X}{len(data):X}" + "".join(f"{b:02X}" for b in data))

    def _read_line(self, timeout: float) -> Optional[bytes]:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                first = self.serial.read(1)
            except serial.SerialException:
                return None
            if not first:
                return None
            if first == b"\r":
                return b""

            payload = bytearray(first)
            while True:
                try:
                    ch = self.serial.read(1)
                except serial.SerialException:
                    return None
                if not ch:
                    break
                if ch == b"\r":
                    return bytes(payload)
                payload.extend(ch)
            return bytes(payload)
        return None

    def read_frame(self, timeout: float = 1.0, ids: Optional[set[int]] = None) -> Optional[CanFrame]:
        wanted = set(ids) if ids is not None else None
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            line = self._read_line(max(0.0, deadline - time.monotonic()))
            if not line:
                continue
            frame = parse_can_line(line)
            if frame is None:
                continue
            if wanted is None or frame.can_id in wanted:
                return frame
        return None

    def enable_log(self) -> None:
        self.send("O")

    def disable_log(self) -> None:
        self.send("C")

    def close(self) -> None:
        if self.serial and self.serial.is_open:
            self.serial.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        self.disable_log()
        self.close()


class CanOpenSdoClient:
    def __init__(
        self,
        bus: UsbCanBus,
        node_id: int,
        timeout: float = 3.0,
        *,
        segment_delay: float = 0.0,
        segment_retries: int = 0,
        segment_timeout: Optional[float] = None,
        sdo_debug: bool = False,
    ) -> None:
        self.bus = bus
        self.node_id = node_id
        self.tx_cobid = CAN_ID_SDO_REQUEST + node_id
        self.rx_cobid = CAN_ID_SDO_RESPONSE + node_id
        self.timeout = timeout
        self.segment_delay = segment_delay
        self.segment_retries = segment_retries
        self.segment_timeout = segment_timeout
        self.sdo_debug = sdo_debug

    def _sdo(
        self,
        payload: bytes,
        *,
        expected_idx: Optional[int] = None,
        expected_sub: Optional[int] = None,
        timeout: Optional[float] = None,
    ) -> CanFrame:
        command = payload[0] if payload else 0x00
        expected_idx = (
            payload[1] | (payload[2] << 8)
            if len(payload) >= 3 and expected_idx is None
            else (expected_idx or 0)
        )
        expected_sub = payload[3] if len(payload) >= 4 and expected_sub is None else (expected_sub or 0x00)
        expected_idx_lo = expected_idx & 0xFF
        expected_idx_hi = (expected_idx >> 8) & 0xFF
        expected_sub_str = f"0x{expected_sub:02X}"

        def _matches_object(data: bytes) -> bool:
            return (
                len(data) >= 4
                and data[1] == expected_idx_lo
                and data[2] == expected_idx_hi
                and data[3] == expected_sub
            )

        is_upload = command == SDO_UPLOAD_REQUEST
        is_segment_data = (command & 0xE0) == 0x00 and command != SDO_UPLOAD_REQUEST
        is_download = is_segment_data or command in (
            SDO_DOWNLOAD_1BYTE,
            SDO_DOWNLOAD_2BYTE,
            SDO_DOWNLOAD_4BYTE,
            SDO_DOWNLOAD_INIT_SEGMENTED,
        )

        if hasattr(self.bus.serial, "reset_input_buffer"):
            try:
                self.bus.serial.reset_input_buffer()
            except Exception:
                pass

        if self.sdo_debug:
            payload_hex = " ".join(f"{byte:02X}" for byte in payload)
            print(f"[SDO TX 0x{self.tx_cobid:03X}] {payload_hex}")
        self.bus.send_frame(self.tx_cobid, payload)
        sdo_timeout = self.timeout if timeout is None else timeout
        deadline = time.monotonic() + sdo_timeout
        while time.monotonic() < deadline:
            response = self.bus.read_frame(timeout=max(0.0, deadline - time.monotonic()), ids={self.rx_cobid})
            if response is None:
                continue
            if not response.data:
                continue

            data = response.data
            if self.sdo_debug:
                data_hex = " ".join(f"{byte:02X}" for byte in data)
                print(f"[SDO RX 0x{response.can_id:03X}] {data_hex}")
            if data[0] == SDO_ABORT:
                abort_index = data[1] | (data[2] << 8) if len(data) >= 3 else 0
                abort_sub = data[3] if len(data) >= 4 else 0
                abort_code = struct.unpack("<I", data[4:8])[0] if len(data) >= 8 else 0
                recoverable_abort = expected_idx in (OTA_CONTROL_OBJECT, 0x5F02) and abort_code in (
                    SDO_ABORT_TIMEOUT,
                    SDO_ABORT_CMD_SPEC_UNKNOWN,
                    SDO_ABORT_TOGGLE_NOT_ALTERNATED,
                )

                if self.sdo_debug:
                    print(
                        f"[SDO ABORT] 0x{abort_index:04X}:0x{abort_sub:02X} code=0x{abort_code:08X}"
                    )
                if len(data) >= 4 and _matches_object(data):
                    raise SdoAbortError(
                        abort_index,
                        abort_sub,
                        abort_code,
                        recoverable=recoverable_abort,
                    )
                raise SdoAbortError(
                    abort_index,
                    abort_sub,
                    abort_code,
                    recoverable=False,
                )

            if is_upload:
                if data[0] in (SDO_UPLOAD_REPLY_1BYTE, SDO_UPLOAD_REPLY_2BYTE, SDO_UPLOAD_REPLY_4BYTE) and _matches_object(
                    data
                ):
                    return response
                continue

            if is_download:
                if is_segment_data:
                    prefix = data[0] & SDO_SEGMENT_DOWNLOAD_RESPONSE_MASK
                    if prefix == SDO_SEGMENT_RESPONSE_EXPECT:
                        return response
                    continue
                if data[0] == 0x60 and _matches_object(data):
                    return response
                continue

            if data[0] == 0x60 and _matches_object(data):
                return response

        raise TimeoutError(
            f"Timeout waiting for SDO response for 0x{self.tx_cobid - CAN_ID_SDO_REQUEST:03X}:"
            f"{expected_sub_str}"
        )

    def write_u8(self, index: int, sub: int, value: int) -> None:
        request = bytes(
            [
                SDO_DOWNLOAD_1BYTE,
                index & 0xFF,
                (index >> 8) & 0xFF,
                sub,
                value & 0xFF,
                0,
                0,
                0,
            ]
        )
        resp = self._sdo(request)
        if resp.data[0] != 0x60:
            raise RuntimeError(f"Unexpected SDO write response: 0x{resp.data[0]:02X}")

    def write_u16(self, index: int, sub: int, value: int) -> None:
        request = bytes(
            [
                SDO_DOWNLOAD_2BYTE,
                index & 0xFF,
                (index >> 8) & 0xFF,
                sub,
            ]
        ) + struct.pack("<H", value & 0xFFFF) + b"\x00\x00"
        resp = self._sdo(request)
        if resp.data[0] != 0x60:
            raise RuntimeError(f"Unexpected SDO write response: 0x{resp.data[0]:02X}")

    def write_u32(self, index: int, sub: int, value: int) -> None:
        request = bytes(
            [
                SDO_DOWNLOAD_4BYTE,
                index & 0xFF,
                (index >> 8) & 0xFF,
                sub,
            ]
        ) + struct.pack("<I", value & 0xFFFFFFFF)
        resp = self._sdo(request)
        if resp.data[0] != 0x60:
            raise RuntimeError(f"Unexpected SDO write response: 0x{resp.data[0]:02X}")

    def read_u8(self, index: int, sub: int) -> int:
        request = bytes([SDO_UPLOAD_REQUEST, index & 0xFF, (index >> 8) & 0xFF, sub, 0, 0, 0, 0])
        resp = self._sdo(request)
        if resp.data[0] != SDO_UPLOAD_REPLY_1BYTE:
            raise RuntimeError(f"Unexpected SDO upload response: 0x{resp.data[0]:02X}")
        return resp.data[4]

    def read_u16(self, index: int, sub: int) -> int:
        request = bytes([SDO_UPLOAD_REQUEST, index & 0xFF, (index >> 8) & 0xFF, sub, 0, 0, 0, 0])
        resp = self._sdo(request)
        if resp.data[0] != SDO_UPLOAD_REPLY_2BYTE:
            raise RuntimeError(f"Unexpected SDO upload response: 0x{resp.data[0]:02X}")
        return struct.unpack("<H", resp.data[4:6])[0]

    def read_u32(self, index: int, sub: int) -> int:
        request = bytes([SDO_UPLOAD_REQUEST, index & 0xFF, (index >> 8) & 0xFF, sub, 0, 0, 0, 0])
        resp = self._sdo(request)
        if resp.data[0] != SDO_UPLOAD_REPLY_4BYTE:
            raise RuntimeError(f"Unexpected SDO upload response: 0x{resp.data[0]:02X}")
        return struct.unpack("<I", resp.data[4:8])[0]

    def write_data_segmented(self, index: int, sub: int, data: bytes) -> None:
        if not data:
            self.write_u32(index, sub, 0)
            return

        if self.segment_delay > 0:
            time.sleep(self.segment_delay)

        self._sdo(
            bytes(
                [
                    SDO_DOWNLOAD_INIT_SEGMENTED,
                    index & 0xFF,
                    (index >> 8) & 0xFF,
                    sub,
                ]
                + list(struct.pack("<I", len(data)))
            )
        )

        toggle = 0
        offset = 0
        length = len(data)
        total_segments = (length + 6) // 7
        segment_no = 0
        while offset < length:
            chunk = min(7, length - offset)
            chunk_data = data[offset : offset + chunk]
            is_last = (offset + chunk) >= length
            n = 7 - chunk
            ccs = (toggle << 4) | (n << 1) | (0x01 if is_last else 0x00)
            request = bytes([ccs]) + chunk_data
            request += b"\x00" * (8 - len(request))
            segment_no += 1
            remaining = length - offset
            if self.sdo_debug:
                print(
                    f"[SEGMENT] no={segment_no}/{total_segments} off={offset} remain={remaining} "
                    f"len={chunk} last={int(is_last)} toggle={toggle}"
                )
            attempts = 0
            while True:
                try:
                    resp = self._sdo(
                        request,
                        expected_idx=index,
                        expected_sub=sub,
                        timeout=self.segment_timeout,
                    )
                    break
                except TimeoutError:
                    attempts += 1
                    if attempts > self.segment_retries:
                        raise
                    if self.sdo_debug:
                        print(
                            f"[WARN] segment timeout: {segment_no}/{total_segments}, attempt={attempts}/{self.segment_retries}, "
                            f"remaining={remaining}, retrying"
                        )
                    if self.segment_delay > 0:
                        time.sleep(self.segment_delay)
                except SdoAbortError as exc:
                    if exc.recoverable:
                        attempts += 1
                        if attempts <= self.segment_retries:
                            if self.sdo_debug:
                                print(
                                    f"[WARN] recoverable abort: segment={segment_no}/{total_segments}, "
                                    f"attempt={attempts}/{self.segment_retries}, code=0x{exc.code:08X}"
                                )
                            if self.segment_delay > 0:
                                time.sleep(self.segment_delay)
                            continue
                    if self.sdo_debug:
                        print(f"[ERROR] segment {segment_no}/{total_segments} abort: {exc}")
                    raise
                except RuntimeError as exc:
                    if self.sdo_debug:
                        print(f"[ERROR] segment {segment_no}/{total_segments} failed: {exc}")
                    raise
            if (resp.data[0] & SDO_SEGMENT_DOWNLOAD_RESPONSE_MASK) != SDO_SEGMENT_RESPONSE_EXPECT:
                raise RuntimeError(f"Unexpected SDO segment response: 0x{resp.data[0]:02X}")
            if ((resp.data[0] >> 4) & 0x01) != toggle:
                raise RuntimeError(
                    "Unexpected SDO toggle bit: expected="
                    f"{toggle}, got={(resp.data[0] >> 4) & 0x01} (data=0x{resp.data[0]:02X})"
                )
            offset += chunk
            toggle ^= 1
            if self.segment_delay > 0:
                time.sleep(self.segment_delay)

    def read_status(self) -> OtaStatus:
        return OtaStatus(
            state=self.read_u8(0x5F01, 1),
            last_error=self.read_u16(0x5F01, 2),
            bytes_received=self.read_u32(0x5F01, 3),
            bytes_written=self.read_u32(0x5F01, 4),
            image_crc32=self.read_u32(0x5F01, 5),
            progress=self.read_u8(0x5F01, 6),
        )


def parse_can_line(line: bytes) -> Optional[CanFrame]:
    if not line or line[0] != ord("t") or len(line) < 5:
        return None
    try:
        can_id = int(line[1:4].decode("ascii"), 16)
        dlc = int(chr(line[4]), 16)
    except (ValueError, UnicodeDecodeError):
        return None

    if not (0 <= dlc <= 8):
        return None

    payload_hex = line[5 : 5 + dlc * 2]
    if len(payload_hex) != dlc * 2:
        return None

    try:
        data = bytes.fromhex(payload_hex.decode("ascii"))
    except ValueError:
        return None

    return CanFrame(can_id=can_id, data=data, timestamp=time.time())


def _find_usb_ports() -> list[str]:
    ports: list[str] = []
    com_ports = serial.tools.list_ports.comports()
    print("Available COM ports with details:")
    for port, desc, hwid in sorted(com_ports):
        print(f"{port}: {desc} [{hwid}]")
        try:
            s_port = serial.Serial(port, 115200, timeout=0.5)
            time.sleep(0.1)
            if s_port.is_open:
                s_port.write('V\r'.encode("ascii"))
                time.sleep(0.1)
                if (s_port.in_waiting > 0):
                    received = s_port.readline().decode("ascii")
                    if 'canable' in received:
                        ports.append(port)
        except Exception as exc:
            continue

    return ports

def detect_port() -> Optional[str]:
    ports = _find_usb_ports()
    return ports[0] if ports and len(ports) == 1 else None


def detect_node_id_by_heartbeat(bus: UsbCanBus, timeout: float = 8.0) -> Optional[int]:
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        frame = bus.read_frame(timeout=0.4)
        if frame is None:
            continue
        if 0x700 <= frame.can_id <= 0x77F:
            return frame.can_id - 0x700
    return None


def parse_node_id(value: str) -> int:
    node_id = int(value, 0)
    if not (1 <= node_id <= 0x7F):
        raise argparse.ArgumentTypeError("Node-ID must be 1..127")
    return node_id


def parse_u32(value: str) -> int:
    parsed = int(value, 0)
    if not (0 <= parsed <= 0xFFFFFFFF):
        raise argparse.ArgumentTypeError("Value must be 0..0xFFFFFFFF")
    return parsed


def _kernel_bin_path(profile: str) -> Path:
    return PROJECT_ROOT / f"{profile}.bin"


def _plc_bin_path(plc_target: str) -> Path:
    filename = f"user-{plc_target.replace('_', '-')}.bin"
    return PROJECT_ROOT / "plc" / "AT32F415" / "build" / plc_target / filename


def _resolve_plc_target(profile: str, explicit_target: Optional[str]) -> str:
    if explicit_target:
        return explicit_target
    target = DEFAULT_PLC_BY_PROFILE.get(profile)
    if not target:
        raise ValueError(f"Неизвестный профиль для выбора PLC: {profile}")
    return target


def _crc32_of_file(path: Path) -> int:
    data = path.read_bytes()
    return binascii.crc32(data) & 0xFFFFFFFF


def _print_ota_diag_5f03(sdo: CanOpenSdoClient) -> None:
    try:
        w1 = sdo.read_u32(0x5F03, 1)
        w2 = sdo.read_u32(0x5F03, 2)
        w3 = sdo.read_u32(0x5F03, 3)
    except Exception as exc:
        print(f"[WARN] failed to read OTA diag (0x5F03): {exc}")
        return

    phase = (w1 >> 24) & 0xFF
    segment_raw = w1 & 0x00FFFFFF
    if segment_raw <= 0x0FFF:
        segment_started = segment_raw
        segment_completed = segment_raw
    else:
        segment_started = (segment_raw >> 12) & 0x0FFF
        segment_completed = segment_raw & 0x0FFF
    segment_lag = (
        (segment_started - segment_completed)
        if segment_started >= segment_completed
        else (0x1000 + segment_started - segment_completed)
    )
    if phase == 0x26:
        result = (w3 >> 24) & 0xFF
        transport_lag = (w3 >> 16) & 0xFF
        tx_fail_delta = (w3 >> 8) & 0xFF
        tx_retry_delta = w3 & 0xFF
        req_total = (w2 >> 16) & 0xFFFF
        resp_total = w2 & 0xFFFF
        print(
            "[DIAG] 0x5F03 "
            f"phase=0x{phase:02X} seg_start={segment_started} seg_done={segment_completed} lag={segment_lag} "
            f"req_total={req_total} resp_total={resp_total} "
            f"transport_lag={transport_lag} tx_fail_delta={tx_fail_delta} tx_retry_delta={tx_retry_delta} "
            f"result=0x{result:02X} "
            f"raw=0x{w1:08X}/0x{w2:08X}/0x{w3:08X}"
        )
    elif phase == 0x27:
        result = (w3 >> 24) & 0xFF
        timeout_delta = (w3 >> 16) & 0xFF
        cmd_delta = (w3 >> 8) & 0xFF
        toggle_delta = w3 & 0xFF
        timeout_total = (w2 >> 16) & 0xFFFF
        cmd_total = w2 & 0xFFFF
        print(
            "[DIAG] 0x5F03 "
            f"phase=0x{phase:02X} seg_start={segment_started} seg_done={segment_completed} lag={segment_lag} "
            f"sdo_abort_timeout_total={timeout_total} sdo_abort_cmd_total={cmd_total} "
            f"sdo_abort_timeout_delta={timeout_delta} sdo_abort_cmd_delta={cmd_delta} sdo_abort_toggle_delta={toggle_delta} "
            f"result=0x{result:02X} "
            f"raw=0x{w1:08X}/0x{w2:08X}/0x{w3:08X}"
        )
    else:
        before_offset = w2
        result = (w3 >> 24) & 0xFF
        count = (w3 >> 8) & 0xFFFF
        after_low = w3 & 0xFF
        print(
            "[DIAG] 0x5F03 "
            f"phase=0x{phase:02X} seg_start={segment_started} seg_done={segment_completed} lag={segment_lag} "
            f"before={before_offset} "
            f"result=0x{result:02X} count={count} after_low=0x{after_low:02X} "
            f"raw=0x{w1:08X}/0x{w2:08X}/0x{w3:08X}"
        )


def _run_single_ota(
    sdo: CanOpenSdoClient,
    *,
    target: int,
    image_type: int,
    image_path: Path,
    session_id: int,
    chunk_size: int,
    timeout: float,
    start_delay: float,
    apply_kernel: bool,
    apply_timeout: float,
) -> bool:
    image_data = image_path.read_bytes()
    size = len(image_data)
    crc32 = _crc32_of_file(image_path)

    print(
        f"[INFO] prepare start: target={target} type={image_type} size={size} crc=0x{crc32:08X}"
    )

    sdo.write_u8(0x5F00, 2, target)
    sdo.write_u8(0x5F00, 3, image_type)
    sdo.write_u32(0x5F00, 4, session_id)
    sdo.write_u32(0x5F00, 5, size)
    sdo.write_u32(0x5F00, 6, crc32)
    try:
        sdo.write_u16(0x5F00, 7, chunk_size)
    except (RuntimeError, TimeoutError):
        print(
            "[WARN] failed to set fwUpdateControl.chunkSize; продолжение с текущим значением прошивки"
        )
    try:
        actual_chunk = sdo.read_u16(0x5F00, 7)
        print(f"[INFO] fwUpdateControl.chunkSize={actual_chunk}")
    except (RuntimeError, TimeoutError) as exc:
        print(f"[WARN] failed to read fwUpdateControl.chunkSize: {exc}")

    sdo.write_u8(0x5F00, 1, OTA_CMD_START)
    pre_status = sdo.read_status()
    print(
        f"[INFO] status before OTA state={pre_status.state} recv={pre_status.bytes_received} written={pre_status.bytes_written} err=0x{pre_status.last_error:04X}"
    )

    if start_delay > 0:
        time.sleep(start_delay)

    try:
        sdo.write_data_segmented(0x5F02, 0, image_data)
    except (RuntimeError, TimeoutError) as exc:
        print(f"[ERROR] OTA data transfer failed: {exc}")
        _print_ota_diag_5f03(sdo)
        return False

    end_abort: Optional[str] = None
    try:
        sdo.write_u8(0x5F00, 1, OTA_CMD_END)
    except (RuntimeError, TimeoutError) as exc:
        end_abort = str(exc)
        print(f"[WARN] OTA_CMD_END returned error: {end_abort}")

    deadline = time.monotonic() + timeout
    status_read_timeouts = 0
    while time.monotonic() < deadline:
        try:
            status = sdo.read_status()
        except TimeoutError as exc:
            status_read_timeouts += 1
            if status_read_timeouts <= 3:
                print(f"[WARN] status read timeout: {exc}")
            time.sleep(0.2)
            continue
        except RuntimeError as exc:
            print(f"[WARN] status read failed: {exc}")
            time.sleep(0.2)
            continue

        status_read_timeouts = 0
        print(
            f"[INFO] status: state={status.state} recv={status.bytes_received} written={status.bytes_written} "
            f"crc=0x{status.image_crc32:08X} progress={status.progress}%"
        )

        if status.state == OTA_STATE_ERROR:
            print(f"[ERROR] OTA target error: 0x{status.last_error:04X}")
            if end_abort is not None:
                print(f"[WARN] END details: {end_abort}")
            _print_ota_diag_5f03(sdo)
            return False
        if status.last_error != 0:
            print(f"[ERROR] LastError=0x{status.last_error:04X}")
            if end_abort is not None:
                print(f"[WARN] END details: {end_abort}")
            _print_ota_diag_5f03(sdo)
            return False

        if status.bytes_written >= size and status.bytes_received >= size:
            if status.last_error != 0:
                return False
            if status.image_crc32 != 0 and status.image_crc32 != crc32:
                print(
                    f"[WARN] imageCRC32 mismatch: expected=0x{crc32:08X}, got=0x{status.image_crc32:08X}"
                )
                _print_ota_diag_5f03(sdo)
                return False
            if end_abort is not None:
                print(f"[WARN] OTA counters reached target, but END failed: {end_abort}")
                _print_ota_diag_5f03(sdo)
                return False
            if target == OTA_TARGET_AT32 and apply_kernel:
                try:
                    sdo.write_u8(0x5F00, 1, OTA_CMD_APPLY)
                except (RuntimeError, TimeoutError) as exc:
                    print(f"[ERROR] OTA_CMD_APPLY failed: {exc}")
                    _print_ota_diag_5f03(sdo)
                    return False

                print(f"[INFO] OTA_CMD_APPLY sent, waiting for node reboot to bootloader ({apply_timeout:.1f}s)")
                apply_deadline = time.monotonic() + apply_timeout
                while time.monotonic() < apply_deadline:
                    try:
                        _ = sdo.read_u8(0x5F01, 1)
                    except Exception:
                        print("[INFO] target left CANopen after OTA_CMD_APPLY")
                        return True
                    time.sleep(0.2)

                print("[ERROR] target stayed in CANopen after OTA_CMD_APPLY timeout")
                _print_ota_diag_5f03(sdo)
                return False
            return True

        time.sleep(0.25)

    print("[ERROR] timeout waiting for OTA completion")
    _print_ota_diag_5f03(sdo)
    return False


def _run_ota_case(args: argparse.Namespace) -> int:
    if args.skip_kernel and args.skip_plc:
        print("[ERROR] both --skip-kernel and --skip-plc are set")
        return 1
    if args.kernel_apply and not args.skip_kernel and not args.skip_plc:
        print("[ERROR] --kernel-apply requires --skip-plc (node leaves CANopen after apply)")
        return 1

    port = args.port or detect_port()
    if port is None:
        print("[ERROR] Port not found")
        return 1

    print(f"[INFO] port={port} baudrate={args.baudrate}")
    if args.node_id is not None:
        print(f"[INFO] node-id={args.node_id}")

    with UsbCanBus(port=port, baudrate=args.baudrate) as bus:
        bus.disable_log()
        bus.send("S5")
        bus.enable_log()
        if args.node_id is None:
            node_id = detect_node_id_by_heartbeat(bus, timeout=10.0)
            if node_id is None:
                print("[ERROR] Heartbeat not found")
                return 1
            print(f"[INFO] detected node-id={hex(node_id)}")
        else:
            node_id = args.node_id

        sdo = CanOpenSdoClient(
            bus=bus,
            node_id=node_id,
            timeout=args.sdo_timeout,
            segment_delay=args.segment_delay,
            segment_retries=args.segment_retries,
            segment_timeout=args.segment_timeout,
            sdo_debug=args.sdo_debug,
        )

        serial = None
        vendor_id = None
        product_id = None
        revision = None
        if args.skip_identity:
            print("[INFO] skip-identity: Read 0x1018 disabled")
        else:
            try:
                vendor_id = sdo.read_u32(0x1018, 1)
                product_id = sdo.read_u32(0x1018, 2)
                revision = sdo.read_u32(0x1018, 3)
                serial = sdo.read_u32(0x1018, 4)
            except Exception as exc:
                print(f"[WARN] failed to read identity: {exc}")

        if serial is None:
            serial = 0x00000000
        if product_id is None:
            product_id = 0x00000000
        if vendor_id is None:
            vendor_id = 0x00000000
        if revision is None:
            revision = 0x00000000

        print(
            f"[INFO] identity: vendor=0x{vendor_id:08X} product=0x{product_id:08X} rev=0x{revision:08X} serial=0x{serial:08X}"
        )

        profile = args.profile
        if profile is None:
            if product_id in DEFAULT_PROFILE_BY_PRODUCT:
                profile = DEFAULT_PROFILE_BY_PRODUCT[product_id]
            else:
                print(
                    f"[WARN] Unknown product-id 0x{product_id:08X}, use default profile {DEFAULT_PROFILE}"
                )
                profile = DEFAULT_PROFILE

        try:
            plc_target = _resolve_plc_target(profile, args.plc_target)
        except ValueError as exc:
            args.skip_plc = True
#            print(f"[ERROR] {exc}")
#            return 1

        if args.skip_kernel:
            print("[WARN] OTA kernel skipped")
        else:
            kernel_bin = args.kernel_bin if args.kernel_bin is not None else _kernel_bin_path(profile)
            if not kernel_bin.exists():
                print(f"[ERROR] kernel bin отсутствует: {kernel_bin}")
                print("       Build firmware and repeat or use --kernel-bin")
                return 1
            print(f"=== OTA: AT32 kernel ===")
            print(f"[INFO] image={kernel_bin}")
            if not _run_single_ota(
                sdo=sdo,
                target=OTA_TARGET_AT32,
                image_type=args.kernel_image_type,
                image_path=kernel_bin,
                session_id=randrange(0, 0xFFFFFFFF),
                chunk_size=args.fw_chunk_size,
                timeout=args.timeout,
                start_delay=args.start_data_delay,
                apply_kernel=args.kernel_apply,
                apply_timeout=args.kernel_apply_timeout,
            ):
                print("[WARN] kernel OTA failed, PLC OTA skipped")
                return 1

        if args.skip_plc:
            print("[WARN] OTA PLC skipped")
            return 0

        plc_bin = args.plc_bin if args.plc_bin is not None else _plc_bin_path(plc_target)
        if not plc_bin.exists():
            print(f"[ERROR] plc bin отсутствует: {plc_bin}")
            print("       Build PLC target and repeat or use --plc-bin")
            return 1

        print(f"=== OTA: PLC ({plc_target}) ===")
        print(f"[INFO] image={plc_bin}")
        if not _run_single_ota(
            sdo=sdo,
            target=OTA_TARGET_ASR,
            image_type=args.plc_image_type,
            image_path=plc_bin,
            session_id=randrange(0, 0xFFFFFFFF),
            chunk_size=args.fw_chunk_size,
            timeout=args.timeout,
            start_delay=args.start_data_delay,
            apply_kernel=False,
            apply_timeout=0.0,
        ):
            return 1

        final_status = sdo.read_status()
        print(
            f"[INFO] final status state={final_status.state} recv={final_status.bytes_received} "
            f"written={final_status.bytes_written} crc=0x{final_status.image_crc32:08X} progress={final_status.progress}%"
        )

        print("[PASS] OTA complete")
        return 0


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="CANopen OTA kernel+PLC check")
    parser.add_argument("--port", default=None, help="Serial port (optional)")
    parser.add_argument(
        "--node-id",
        type=parse_node_id,
        default=None,
        help="CAN node-ID (1..0x7F)",
    )
    parser.add_argument("--baudrate", type=int, default=115200)
    parser.add_argument("--profile", default=None, help="AT32 profile")
    parser.add_argument("--kernel-bin", type=Path, default=None, help="kernel bin file name")
    parser.add_argument("--plc-bin", type=Path, default=None, help="plc bin file name")
    parser.add_argument("--plc-target", default=None, help="PLC target")
    parser.add_argument("--skip-kernel", action="store_true", help="Skip kernel OTA")
    parser.add_argument("--skip-plc", action="store_true", help="Skip PLC OTA")
    parser.add_argument(
        "--skip-identity",
        action="store_true",
        help="Skip read 0x1018 before OTA",
    )
    parser.add_argument(
        "--kernel-apply",
        action="store_true",
        help="After OTA_CMD_END for kernel send OTA_CMD_APPLY (0x06) and wait to enter bootloader",
    )
    parser.add_argument(
        "--kernel-apply-timeout",
        type=float,
        default=6.0,
        help="Timeout for node to exit CANopen after OTA_CMD_APPLY (sec)",
    )
    parser.add_argument("--kernel-image-type", type=parse_u32, default=OTA_IMAGE_TYPE_KERNEL)
    parser.add_argument("--plc-image-type", type=parse_u32, default=OTA_IMAGE_TYPE_PLC)
    parser.add_argument(
        "--timeout",
        type=float,
        default=120.0,
        help="OTA completion timeout (sec)",
    )
    parser.add_argument(
        "--segment-delay",
        type=float,
        default=0.0,
        help="Pause between segment SDO frames 0x5F02 in seconds",
    )
    parser.add_argument(
        "--segment-retries",
        type=int,
        default=1,
        help="Number of segment repetitions when SDO timeout",
    )
    parser.add_argument(
        "--start-data-delay",
        type=float,
        default=0.1,
        help="Pause between OTA_CMD_START and the first segment 0x5F02 in seconds",
    )
    parser.add_argument(
        "--sdo-timeout",
        type=float,
        default=10.0,
        help="Timeout per SDO exchange (sec)",
    )
    parser.add_argument(
        "--segment-timeout",
        type=float,
        default=0.8,
        help="Timeout for segment SDO exchange (sec), must be less than the SDO server timeout",
    )
    parser.add_argument(
        "--fw-chunk-size",
        type=parse_u32,
        default=128,
        help="fwUpdateControl.chunkSize (5F00:7), 0..0xFFFF",
    )
    parser.add_argument(
        "--sdo-debug",
        action="store_true",
        help="Log raw SDO frames (requests/responses)",
    )
    return parser


def main() -> int:
    args = _build_parser().parse_args()
    return _run_ota_case(args)


if __name__ == "__main__":
    raise SystemExit(main())
