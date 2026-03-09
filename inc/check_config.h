#ifndef CHECK_CONFIG_H
#define CHECK_CONFIG_H

// Этот файл поможет убедиться, что конфигурация правильная

#if !defined(CO_CONFIG_SDO_SRV)
#error "CO_CONFIG_SDO_SRV not defined!"
#endif

#if !defined(CO_CONFIG_SDO_SRV_BUFFER_SIZE)
#error "CO_CONFIG_SDO_SRV_BUFFER_SIZE not defined!"
#endif

#if (CO_CONFIG_SDO_SRV_BUFFER_SIZE < 1024)
#error "CO_CONFIG_SDO_SRV_BUFFER_SIZE too small!"
#endif

#endif /* CHECK_CONFIG_H */
