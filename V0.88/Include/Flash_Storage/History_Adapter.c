#include "History_Adapter.h"
#include "nrf_log.h"

// Variable externa para mantener compatibilidad
extern store_flash Flash_array;
extern uint16_t    History_Position;

ret_code_t         get_history_record(uint16_t index, store_History *record)
{
    if (record == NULL)
    {
        return NRF_ERROR_NULL;
    }

    return history_get_record(index, record);
}

ret_code_t set_history_record(uint16_t index, const store_History *record)
{
    if (record == NULL)
    {
        return NRF_ERROR_NULL;
    }

    NRF_LOG_RAW_INFO("\n>>> AGREGANDO HISTORIAL - Índice: %d", index);

    // Si es un nuevo registro (índice actual), añadirlo
    if (index == History_Position)
    {
        ret_code_t rc = history_add_record(record);
        if (rc == NRF_SUCCESS)
        {
            // NOTA: history_add_record() ya actualiza History_Position y Flash_array.last_history
            // No necesitamos incrementar aquí para evitar doble incremento

            // Verificar si hemos llegado al límite
            if (History_Position >= MAX_HISTORY_RECORDS)
            {
                NRF_LOG_RAW_INFO("\n\nSe ha alcanzado el límite máximo de historiales: %d",
                                 MAX_HISTORY_RECORDS);
                // Mantener History_Position en el límite para evitar sobrescribir
                History_Position         = MAX_HISTORY_RECORDS;
                Flash_array.last_history = MAX_HISTORY_RECORDS;
            }

            // Diagnóstico post-agregado
            NRF_LOG_RAW_INFO("\n>>> HISTORIAL AGREGADO EXITOSAMENTE");
            debug_print_history_sync_status();
        }
        else
        {
            NRF_LOG_RAW_INFO("\n>>> ERROR AL AGREGAR HISTORIAL: %d", rc);
        }
        return rc;
    }
    else
    {
        // Para actualizaciones de registros existentes, necesitarías
        // implementar una función de actualización en el history manager
        NRF_LOG_RAW_INFO("Actualización de registro existente no implementada");
        return NRF_ERROR_NOT_SUPPORTED;
    }
}

uint16_t get_last_history_index(void)
{
    return Flash_array.last_history;
}

void set_last_history_index(uint16_t index)
{
    // Sincronizar ambos sistemas
    Flash_array.last_history = index;
    History_Position         = index;

    // Sincronizar también el nuevo sistema de gestión
    extern history_manager_t g_history_manager;
    g_history_manager.total_records    = index;
    g_history_manager.next_flash_index = index;
    g_history_manager.current_page     = index / HISTORY_RECORDS_PER_PAGE;

    NRF_LOG_RAW_INFO("SINCRONIZACION: Actualizados todos los contadores a %d", index);
}

ret_code_t get_last_history_record(store_History *record)
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
        // Sincronizar ambos sistemas a cero
        History_Position             = 0;
        Flash_array.last_history     = 0;
        Flash_array.Sending_Position = 0;

        // El history_clear_all() ya resetea g_history_manager, pero para estar seguros:
        extern history_manager_t g_history_manager;
        g_history_manager.total_records    = 0;
        g_history_manager.next_flash_index = 0;
        g_history_manager.current_page     = 0;

        NRF_LOG_RAW_INFO("SINCRONIZACION: Todos los historiales borrados, contadores en 0");
    }
    return rc;
}

uint16_t get_total_history_count(void)
{
    return history_get_total_count();
}

// Función de diagnóstico para verificar sincronización
void debug_print_history_sync_status(void)
{
    extern history_manager_t g_history_manager;

    NRF_LOG_RAW_INFO("\n=== ESTADO DE SINCRONIZACION DE HISTORIALES ===");
    NRF_LOG_RAW_INFO("\nSistema Legacy:");
    NRF_LOG_RAW_INFO("\n  History_Position: %d", History_Position);
    NRF_LOG_RAW_INFO("\n  Flash_array.last_history: %d", Flash_array.last_history);
    NRF_LOG_RAW_INFO("\n  Size_Memory_History (define): %d", Size_Memory_History);

    NRF_LOG_RAW_INFO("\nSistema Nuevo (g_history_manager):");
    NRF_LOG_RAW_INFO("\n  total_records: %d", g_history_manager.total_records);
    NRF_LOG_RAW_INFO("\n  next_flash_index: %d", g_history_manager.next_flash_index);
    NRF_LOG_RAW_INFO("\n  current_cache_size: %d", g_history_manager.current_cache_size);
    NRF_LOG_RAW_INFO("\n  current_page: %d", g_history_manager.current_page);

    // Verificar sincronización
    bool sync_ok = (History_Position == Flash_array.last_history) &&
                   (History_Position == g_history_manager.total_records) &&
                   (History_Position == g_history_manager.next_flash_index);

    if (sync_ok)
    {
        NRF_LOG_RAW_INFO("\n✅ SINCRONIZACION: OK - Todos los valores coinciden");
    }
    else
    {
        NRF_LOG_RAW_INFO("\n❌ SINCRONIZACION: ERROR - Valores desincronizados!");
        NRF_LOG_RAW_INFO("\n   Diferencia History_Position vs total_records: %d",
                         History_Position - g_history_manager.total_records);
        NRF_LOG_RAW_INFO("\n   Diferencia History_Position vs next_flash_index: %d",
                         History_Position - g_history_manager.next_flash_index);
    }
    NRF_LOG_RAW_INFO("\n===============================================\n");
}
