#include "History_Adapter.h"
#include "nrf_log.h"

// Variable externa para mantener compatibilidad
extern store_flash Flash_array;
extern uint16_t History_Position;

ret_code_t get_history_record(uint16_t index, store_History* record)
{
    if (record == NULL)
    {
        return NRF_ERROR_NULL;
    }
    
    return history_get_record(index, record);
}

ret_code_t set_history_record(uint16_t index, const store_History* record)
{
    if (record == NULL)
    {
        return NRF_ERROR_NULL;
    }
    
    // Si es un nuevo registro (índice actual), añadirlo
    if (index == History_Position)
    {
        ret_code_t rc = history_add_record(record);
        if (rc == NRF_SUCCESS)
        {
            History_Position++;
            if (History_Position >= Size_Memory_History)
            {
                History_Position = 0; // Modo circular
            }
            Flash_array.last_history = History_Position;
        }
        return rc;
    }
    else
    {
        // Para actualizaciones de registros existentes, necesitarías
        // implementar una función de actualización en el history manager
        NRF_LOG_WARNING("Actualización de registro existente no implementada");
        return NRF_ERROR_NOT_SUPPORTED;
    }
}

uint16_t get_last_history_index(void)
{
    return Flash_array.last_history;
}

void set_last_history_index(uint16_t index)
{
    Flash_array.last_history = index;
    History_Position = index;
}

ret_code_t get_last_history_record(store_History* record)
{
    if (record == NULL)
    {
        return NRF_ERROR_NULL;
    }
    
    return history_get_last_record(record);
}

ret_code_t migrate_existing_history_data(void)
{
    // Esta función se usaría para migrar datos del formato anterior
    // si ya tienes datos guardados en el formato viejo
    NRF_LOG_INFO("Migración de datos de historial - no implementada en este ejemplo");
    return NRF_SUCCESS;
}

ret_code_t clear_all_history(void)
{
    ret_code_t rc = history_clear_all();
    if (rc == NRF_SUCCESS)
    {
        History_Position = 0;
        Flash_array.last_history = 0;
        Flash_array.Sending_Position = 0;
    }
    return rc;
}

uint16_t get_total_history_count(void)
{
    return history_get_total_count();
}
