# CANopen SDO Bootloader for AT32

A robust CANopen bootloader for ARTERY AT32 microcontrollers, implementing firmware updates via SDO (Service Data Object) protocols.
Supports both segmented and block SDO transfers for efficient and reliable in-field updates.

## ✨ Features

- **Full CANopen DS-302 compliance** - Standardized communication profile
- **Dual SDO transfer modes**:
  - Segmented transfer for compatibility
  - **Block transfer** for high-speed updates (up to 1.5x faster)
- **Multi-vendor support** - Compatible with any CANopen master supporting SDO
- **Flash memory management**:
  - Sector/Page erase before write
  - Address range validation (prevents overwriting bootloader)
  - Configurable application area
- **Python CLI tool** - Simple host-side utility for firmware upload
- **RTOS-ready** - Built on CANopenNode with freeRTOS support
- **Error handling** - Full SDO abort codes and flash operation verification

## 🛠 Hardware Requirements

- **MCU**: ARTERY AT32 series (AT32F403/405/407/415 tested)
- **CAN transceiver**: Compatible with 3.3V logic (e.g., TJA1050, SN65HVD230)
- **Clock**: 8-16 MHz external crystal (for CAN timing accuracy)
