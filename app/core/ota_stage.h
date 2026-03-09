#ifndef OTA_STAGE_H
#define OTA_STAGE_H

#include <stdint.h>

/*
 * CAN-only OTA staging layout for AT32F415 single-bank flash.
 * App image (IROM1) is limited so a second staging slot can coexist
 * in the same bank without overlap during swap.
 */
#define OTA_FLASH_BASE_ADDR         0x08000000U
#define OTA_KERNEL_REGION_SIZE      0x0002E000U
#define OTA_STAGE_REGION_SIZE       0x00017000U
#define OTA_STAGE_REGION_START      (OTA_FLASH_BASE_ADDR + OTA_KERNEL_REGION_SIZE - OTA_STAGE_REGION_SIZE)
#define OTA_STAGE_REGION_END        (OTA_STAGE_REGION_START + OTA_STAGE_REGION_SIZE)
#define OTA_KERNEL_MAX_IMAGE_SIZE   OTA_STAGE_REGION_SIZE
#define OTA_FLASH_PAGE_SIZE_BYTES   0x00000800U

#ifdef __cplusplus
extern "C" {
#endif

void OtaBoot_ApplyFromStage(uint32_t imageSize, uint32_t expectedCrc32);

#ifdef __cplusplus
}
#endif

#endif /* OTA_STAGE_H */
