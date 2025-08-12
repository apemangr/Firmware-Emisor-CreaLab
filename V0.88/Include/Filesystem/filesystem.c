#include "filesystem.h"
#include "Variables.h"  // Para tener acceso a store_flash y Reset_Index
#include "fds.h"
#include "nrf_log.h"
#include "app_error.h"
#include "nrf_delay.h"
#include <string.h>

// Variables globales
fds_config_data_t g_config_data;
fds_reset_data_t g_reset_data;
extern store_flash Flash_Factory;  // Declarada en Variables.h
extern uint8_t Reset_Index;        // Declarada en Variables.h
fds_state_t fds_current_state = FDS_STATE_UNINITIALIZED;

// Variables internas para control de operaciones asíncronas
static volatile bool m_fds_initialized = false;
static volatile bool m_operation_pending = false;

// Declaraciones de funciones internas
static void inicializar_datos_de_fabrica(void);

/**@brief FDS event handler.
 */
static void fds_evt_handler(fds_evt_t const * p_evt)
{
    switch (p_evt->id)
    {
        case FDS_EVT_INIT:
            if (p_evt->result == NRF_SUCCESS)
            {
                NRF_LOG_RAW_INFO("\nFDS initialization successful.");
                m_fds_initialized = true;
                fds_current_state = FDS_STATE_INITIALIZED;
            }
            else
            {
                NRF_LOG_RAW_INFO("\nFDS initialization failed.");
                fds_current_state = FDS_STATE_ERROR;
            }
            break;

        case FDS_EVT_WRITE:
            if (p_evt->result == NRF_SUCCESS)
            {
                NRF_LOG_RAW_INFO("\nFDS write successful. File ID: 0x%04X, Record key: 0x%04X", 
                            p_evt->write.file_id, p_evt->write.record_key);
            }
            else
            {
                NRF_LOG_RAW_INFO("\nFDS write failed.");
            }
            m_operation_pending = false;
            break;

        case FDS_EVT_UPDATE:
            if (p_evt->result == NRF_SUCCESS)
            {
                NRF_LOG_RAW_INFO("\nFDS update successful. File ID: 0x%04X, Record key: 0x%04X", 
                            p_evt->write.file_id, p_evt->write.record_key);
            }
            else
            {
                NRF_LOG_ERROR("FDS update failed.");
            }
            m_operation_pending = false;
            break;

        case FDS_EVT_DEL_RECORD:
            if (p_evt->result == NRF_SUCCESS)
            {
                NRF_LOG_INFO("FDS record delete successful.");
            }
            else
            {
                NRF_LOG_ERROR("FDS record delete failed.");
            }
            m_operation_pending = false;
            break;

        case FDS_EVT_DEL_FILE:
            if (p_evt->result == NRF_SUCCESS)
            {
                NRF_LOG_INFO("FDS file delete successful.");
            }
            else
            {
                NRF_LOG_ERROR("FDS file delete failed.");
            }
            m_operation_pending = false;
            break;

        case FDS_EVT_GC:
            if (p_evt->result == NRF_SUCCESS)
            {
                NRF_LOG_INFO("FDS garbage collection successful.");
            }
            else
            {
                NRF_LOG_ERROR("FDS garbage collection failed.");
            }
            m_operation_pending = false;
            break;

        default:
            break;
    }
}

/**@brief Wait for an FDS operation to complete.
 */
static void wait_for_fds_ready(void)
{
    while (m_operation_pending)
    {
        #ifdef SOFTDEVICE_PRESENT
            (void) sd_app_evt_wait();
        #else
            __WFE();
        #endif
    }
}

/**@brief Initialize the filesystem (FDS).
 */
ret_code_t inicializar_fds(void)
{
    ret_code_t ret;

    if (fds_current_state != FDS_STATE_UNINITIALIZED)
    {
        return NRF_SUCCESS; // Ya inicializado
    }

    fds_current_state = FDS_STATE_INITIALIZING;

    // Registrar el event handler
    ret = fds_register(fds_evt_handler);
    if (ret != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("Failed to register FDS event handler: %s", nrf_strerror_get(ret));
        fds_current_state = FDS_STATE_ERROR;
        return ret;
    }

    // Inicializar FDS
    ret = fds_init();
    if (ret != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("Failed to initialize FDS: %s", nrf_strerror_get(ret));
        fds_current_state = FDS_STATE_ERROR;
        return ret;
    }

    // Esperar a que se complete la inicialización
    while (!m_fds_initialized && fds_current_state == FDS_STATE_INITIALIZING)
    {
        #ifdef SOFTDEVICE_PRESENT
            (void) sd_app_evt_wait();
        #else
            __WFE();
        #endif
    }

    if (fds_current_state == FDS_STATE_INITIALIZED)
    {
        NRF_LOG_INFO("FDS filesystem initialized successfully.");
        
        // Inicializar datos de fábrica
        inicializar_datos_de_fabrica();
        
        return NRF_SUCCESS;
    }
    else
    {
        return NRF_ERROR_INTERNAL;
    }
}

/**@brief Check if filesystem is initialized.
 */
bool filesystem_is_initialized(void)
{
    return (fds_current_state == FDS_STATE_INITIALIZED);
}

/**@brief Initialize factory default data.
 */
static void inicializar_datos_de_fabrica(void)
{
    memset(&Flash_Factory, 0, sizeof(Flash_Factory));

    Flash_Factory.company[0] = 0x33;
    Flash_Factory.company[1] = 0x22;

    // Sleep time configuration
    Flash_Factory.sleep_time[0] = 0x00;
    Flash_Factory.sleep_time[1] = 0xEA;
    Flash_Factory.sleep_time[2] = 0x60;

    // Advertising time configuration  
    Flash_Factory.adv_time[0] = 0x03;
    Flash_Factory.adv_time[1] = 0xE8;

    Flash_Factory.Type_sensor = 0x10;
    Flash_Factory.offset_plate_bolt = 0x80;
    Flash_Factory.Offset_sensor_bolt = 0x7d;
    Flash_Factory.Offset_sensor_cut = 0x80;
    Flash_Factory.Sensibility = 15;
    Flash_Factory.last_history = 0;
    Flash_Factory.Sending_Position = 0;
    Flash_Factory.second = 1;
    Flash_Factory.minute = 1;
    Flash_Factory.hour = 1;
    Flash_Factory.date = 1;
    Flash_Factory.month = 1;
    Flash_Factory.year = 2024;
    Flash_Factory.Version[0] = 0;
    Flash_Factory.Version[1] = 8;
    Flash_Factory.Version[2] = 8;

    NRF_LOG_INFO("Factory data initialized.");
}

/**@brief Write configuration data to flash.
 */
ret_code_t guardar_configuracion_en_memoria(const store_flash* p_config)
{
    if (!filesystem_is_initialized() || p_config == NULL)
    {
        return NRF_ERROR_INVALID_STATE;
    }

    fds_record_desc_t record_desc;
    fds_find_token_t find_tok = {0};
    
    fds_record_t record = 
    {
        .file_id = CONFIG_FILE_ID,
        .key = CONFIG_RECORD_KEY,
        .data.p_data = p_config,
        .data.length_words = (sizeof(store_flash) + 3) / 4 // Convert bytes to words
    };

    ret_code_t ret;
    
    // Buscar si ya existe el record
    ret = fds_record_find(CONFIG_FILE_ID, CONFIG_RECORD_KEY, &record_desc, &find_tok);
    
    m_operation_pending = true;
    
    if (ret == NRF_SUCCESS)
    {
        // El record existe, actualizarlo
        ret = fds_record_update(&record_desc, &record);
    }
    else
    {
        // El record no existe, crear uno nuevo
        ret = fds_record_write(&record_desc, &record);
    }

    if (ret == NRF_SUCCESS)
    {
        wait_for_fds_ready();
        NRF_LOG_INFO("Configuration data written to flash.");
    }
    else
    {
        m_operation_pending = false;
        NRF_LOG_ERROR("Failed to write configuration data: %s", nrf_strerror_get(ret));
    }

    return ret;
}

/**@brief Read configuration data from flash.
 */
ret_code_t leer_configuracion_desde_memoria(store_flash* p_config)
{
    if (!filesystem_is_initialized() || p_config == NULL)
    {
        return NRF_ERROR_INVALID_STATE;
    }

    fds_flash_record_t flash_record;
    fds_record_desc_t record_desc;
    fds_find_token_t find_tok = {0};

    ret_code_t ret = fds_record_find(CONFIG_FILE_ID, CONFIG_RECORD_KEY, &record_desc, &find_tok);
    
    if (ret == NRF_SUCCESS)
    {
        ret = fds_record_open(&record_desc, &flash_record);
        if (ret == NRF_SUCCESS)
        {
            memcpy(p_config, flash_record.p_data, sizeof(store_flash));
            ret = fds_record_close(&record_desc);
            NRF_LOG_INFO("Configuration data read from flash.");
        }
        else
        {
            NRF_LOG_ERROR("Failed to open configuration record.");
        }
    }
    else
    {
        NRF_LOG_WARNING("Configuration record not found.");
    }

    return ret;
}