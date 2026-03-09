#ifndef FIRMWARE_LOADER_H
#define FIRMWARE_LOADER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "app_config.h"

typedef struct {
    uint8_t buffer[FIRMWARE_BUFFER_SIZE];
    uint32_t size;
    uint32_t crc;
    volatile uint8_t status;
    volatile uint8_t command;
    volatile bool new_data_available;
    volatile bool bitrate_change_requested;
    volatile uint16_t new_bitrate;
    volatile uint16_t bitrate_delay;
} firmware_loader_t;

void firmware_loader_init(firmware_loader_t *fw);
void firmware_loader_process(firmware_loader_t *fw);
uint32_t firmware_get_size(firmware_loader_t *fw);
void firmware_set_size(firmware_loader_t *fw, uint32_t size);
uint8_t* firmware_get_buffer(firmware_loader_t *fw);
void firmware_notify_new_data(firmware_loader_t *fw);
void firmware_request_bitrate_change(firmware_loader_t *fw, uint16_t bitrate, uint16_t delay);

#ifdef __cplusplus
}
#endif

#endif /* FIRMWARE_LOADER_H */
