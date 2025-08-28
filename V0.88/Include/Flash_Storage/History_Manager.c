#include "History_Manager.h"
#include "nrf_fstorage.h"
#include "nrf_fstorage_nvmc.h"
#include "nrf_fstorage_sd.h"
#include "nrf_log.h"
#include "app_error.h"
#include <string.h>

// Instancia global del gestor
history_manager_t g_history_manager;

// Instancia de fstorage para historiales
static void history_fstorage_evt_handler(nrf_fstorage_evt_t *p_evt);

NRF_FSTORAGE_DEF(nrf_fstorage_t history_fstorage) = {
    .evt_handler = history_fstorage_evt_handler,
    .start_addr  = HISTORY_FLASH_START_ADDR,
    .end_addr    = HISTORY_FLASH_END_ADDR,
};

static void history_fstorage_evt_handler(nrf_fstorage_evt_t *p_evt)
{
    if (p_evt->result != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("Historia Flash operation failed");
        return;
    }

    switch (p_evt->id)
    {
    case NRF_FSTORAGE_EVT_WRITE_RESULT:
        NRF_LOG_DEBUG("Historia: escribio %d bytes en direccion 0x%x", p_evt->len, p_evt->addr);
        break;

    case NRF_FSTORAGE_EVT_ERASE_RESULT:
        NRF_LOG_DEBUG("Historia: borro pagina en direccion 0x%x", p_evt->addr);
        break;

    default:
        break;
    }
}

static void history_wait_for_flash_ready(void)
{
    while (nrf_fstorage_is_busy(&history_fstorage))
    {
#ifdef SOFTDEVICE_PRESENT
        (void)sd_app_evt_wait();
#else
        __WFE();
#endif
    }
}

ret_code_t history_manager_init(void)
{
    ret_code_t rc;

    // Verificar tamaño de estructura para flash
    // NRF_LOG_INFO("Tamaño de store_History: %d bytes", sizeof(store_History));
    if (sizeof(store_History) % 4 != 0)
    {
        NRF_LOG_WARNING("store_History no es múltiplo de 4 bytes, puede causar problemas");
    }

    // Inicializar fstorage para historiales
    nrf_fstorage_api_t *p_fs_api = &nrf_fstorage_sd;
    rc                           = nrf_fstorage_init(&history_fstorage, p_fs_api, NULL);
    VERIFY_SUCCESS(rc);

    // Inicializar el gestor
    memset(&g_history_manager, 0, sizeof(g_history_manager));
    g_history_manager.current_cache_size = 0;

    // SINCRONIZACIÓN CON SISTEMA LEGACY: Leer valores persistidos desde Flash_array
    extern store_flash Flash_array;
    extern uint16_t    History_Position;

    // Sincronizar con los valores persistidos en flash
    if (Flash_array.last_history > 0 && Flash_array.last_history <= MAX_HISTORY_RECORDS)
    {
        g_history_manager.total_records    = Flash_array.last_history;
        g_history_manager.next_flash_index = Flash_array.last_history;
        g_history_manager.current_page     = Flash_array.last_history / HISTORY_RECORDS_PER_PAGE;

        NRF_LOG_RAW_INFO("\nSINCRONIZACION: History Manager inicializado con datos existentes:");
        NRF_LOG_RAW_INFO("\n  Flash_array.last_history: %d", Flash_array.last_history);
        NRF_LOG_RAW_INFO("\n  g_history_manager.total_records: %d",
                         g_history_manager.total_records);
        NRF_LOG_RAW_INFO("\n  g_history_manager.next_flash_index: %d",
                         g_history_manager.next_flash_index);
    }
    else
    {
        // Inicialización en limpio
        g_history_manager.total_records    = 0;
        g_history_manager.next_flash_index = 0;
        g_history_manager.current_page     = 0;

        NRF_LOG_RAW_INFO("SINCRONIZACION: History Manager inicializado desde cero");
    }

    NRF_LOG_RAW_INFO("\nHistory Manager inicializado correctamente\n\n");
    return NRF_SUCCESS;
}

ret_code_t history_add_record(const store_History *record)
{
    if (record == NULL)
    {
        NRF_LOG_ERROR("history_add_record: record es NULL");
        return NRF_ERROR_NULL;
    }

    NRF_LOG_RAW_INFO("\nAgregando registro al historial. Total actual: %d",
                     g_history_manager.total_records);

    // Verificar si hemos llegado al límite máximo ANTES de hacer cualquier cambio
    if (g_history_manager.next_flash_index >= MAX_HISTORY_RECORDS)
    {
        NRF_LOG_RAW_INFO("\n\nHistorial lleno - no se pueden guardar mas registros. Maximo: %d\n",
                         MAX_HISTORY_RECORDS);
        return NRF_ERROR_NO_MEM;
    }

    // Buscar slot libre en caché
    uint8_t cache_slot = history_get_free_cache_slot();
    if (cache_slot >= HISTORY_CACHE_SIZE)
    {
        // Cache lleno, buscar el slot más antiguo que se pueda reutilizar
        cache_slot = 0; // Por simplicidad, usar el primer slot

        // Si el slot tiene datos sucios, escribirlos primero
        if (g_history_manager.cache[cache_slot].state == CACHE_DIRTY)
        {
            NRF_LOG_DEBUG("Cache lleno, escribiendo slot %d sucio antes de reusar", cache_slot);
            ret_code_t rc = history_write_cache_to_flash(cache_slot);
            if (rc != NRF_SUCCESS)
            {
                NRF_LOG_ERROR("Error escribiendo cache lleno: %d", rc);
                return rc;
            }
        }
        else
        {
            NRF_LOG_DEBUG("Cache lleno, reusando slot %d (estado: %d)", cache_slot,
                          g_history_manager.cache[cache_slot].state);
        }
    }

    // Validar dirección de flash antes de escribir
    uint32_t flash_addr =
        HISTORY_FLASH_START_ADDR + (g_history_manager.next_flash_index * sizeof(store_History));
    if (flash_addr + sizeof(store_History) > HISTORY_FLASH_END_ADDR)
    {
        NRF_LOG_ERROR("Dirección de flash fuera de rango: 0x%x", flash_addr);
        return NRF_ERROR_NO_MEM;
    }

    // Copiar registro al caché
    memcpy(&g_history_manager.cache[cache_slot].record, record, sizeof(store_History));
    g_history_manager.cache[cache_slot].state       = CACHE_DIRTY;
    g_history_manager.cache[cache_slot].flash_index = g_history_manager.next_flash_index;

    // Actualizar contadores - solo incrementar cache_size si es un slot nuevo
    if (cache_slot >= g_history_manager.current_cache_size)
    {
        g_history_manager.current_cache_size = cache_slot + 1;
    }
    g_history_manager.total_records++;
    g_history_manager.next_flash_index++;

    // SINCRONIZACIÓN CON SISTEMA LEGACY: Actualizar variables globales
    extern store_flash Flash_array;
    extern uint16_t    History_Position;

    History_Position         = g_history_manager.next_flash_index;
    Flash_array.last_history = g_history_manager.next_flash_index;

    // Escribir inmediatamente a flash para mantener persistencia
    ret_code_t rc = history_write_cache_to_flash(cache_slot);
    if (rc != NRF_SUCCESS)
    {
        NRF_LOG_ERROR("Error escribiendo a flash slot %d: %d", cache_slot, rc);
        // Revertir cambios en caso de error
        g_history_manager.cache[cache_slot].state = CACHE_EMPTY;
        g_history_manager.total_records--;
        g_history_manager.next_flash_index--;
        // Revertir también las variables legacy
        History_Position--;
        Flash_array.last_history--;
        return rc;
    }

    NRF_LOG_RAW_INFO("\n\nNuevo registro anadido al historial. Total: %d, Flash index: %d",
                     g_history_manager.total_records,
                     g_history_manager.cache[cache_slot].flash_index);
    return NRF_SUCCESS;
}

ret_code_t history_get_record(uint16_t index, store_History *record)
{
    if (record == NULL || index >= MAX_HISTORY_RECORDS)
    {
        return NRF_ERROR_INVALID_PARAM;
    }

    // Buscar en caché primero
    uint8_t cache_slot = history_find_cache_slot(index);
    if (cache_slot < HISTORY_CACHE_SIZE)
    {
        memcpy(record, &g_history_manager.cache[cache_slot].record, sizeof(store_History));
        history_update_cache_lru(cache_slot);
        return NRF_SUCCESS;
    }

    // No está en caché, leer desde flash
    ret_code_t rc = history_read_from_flash(index, record);
    if (rc == NRF_SUCCESS)
    {
        // Opcionalmente, añadir al caché para futuras lecturas
        uint8_t free_slot = history_get_free_cache_slot();
        if (free_slot < HISTORY_CACHE_SIZE)
        {
            memcpy(&g_history_manager.cache[free_slot].record, record, sizeof(store_History));
            g_history_manager.cache[free_slot].state       = CACHE_CLEAN;
            g_history_manager.cache[free_slot].flash_index = index;
            g_history_manager.current_cache_size++;
        }
    }

    return rc;
}

ret_code_t history_get_last_record(store_History *record)
{
    if (g_history_manager.total_records == 0)
    {
        return NRF_ERROR_NOT_FOUND;
    }

    // El último registro siempre es el anterior al next_flash_index
    uint16_t last_index = g_history_manager.next_flash_index - 1;

    return history_get_record(last_index, record);
}

ret_code_t history_clear_all(void)
{
    // Limpiar caché
    memset(&g_history_manager.cache, 0, sizeof(g_history_manager.cache));
    g_history_manager.current_cache_size = 0;
    g_history_manager.total_records      = 0;
    g_history_manager.next_flash_index   = 0;

    // Borrar todas las páginas de flash
    for (uint16_t page = 0;
         page < (HISTORY_FLASH_END_ADDR - HISTORY_FLASH_START_ADDR) / HISTORY_PAGE_SIZE; page++)
    {
        ret_code_t rc = history_erase_page(page);
        VERIFY_SUCCESS(rc);
    }

    NRF_LOG_INFO("Historial completamente borrado");
    return NRF_SUCCESS;
}

ret_code_t history_flush_cache(void)
{
    for (uint8_t i = 0; i < HISTORY_CACHE_SIZE; i++)
    {
        if (g_history_manager.cache[i].state == CACHE_DIRTY)
        {
            ret_code_t rc = history_write_cache_to_flash(i);
            VERIFY_SUCCESS(rc);
        }
    }

    return NRF_SUCCESS;
}

uint16_t history_get_total_count(void)
{
    return g_history_manager.total_records;
}

ret_code_t history_get_record_range(uint16_t start_index, uint16_t count, store_History *records)
{
    if (records == NULL || count == 0)
    {
        return NRF_ERROR_INVALID_PARAM;
    }

    for (uint16_t i = 0; i < count; i++)
    {
        ret_code_t rc = history_get_record(start_index + i, &records[i]);
        if (rc != NRF_SUCCESS)
        {
            return rc;
        }
    }

    return NRF_SUCCESS;
}

// ========== FUNCIONES INTERNAS ==========

static ret_code_t history_write_cache_to_flash(uint8_t cache_index)
{
    NRF_LOG_DEBUG("Iniciando escritura a flash, cache_index: %d", cache_index);

    if (cache_index >= HISTORY_CACHE_SIZE)
    {
        NRF_LOG_ERROR("Cache index inválido: %d", cache_index);
        return NRF_ERROR_INVALID_PARAM;
    }

    if (g_history_manager.cache[cache_index].state != CACHE_DIRTY)
    {
        NRF_LOG_WARNING("Cache slot %d no está sucio, estado: %d", cache_index,
                        g_history_manager.cache[cache_index].state);
        return NRF_ERROR_INVALID_STATE;
    }

    uint16_t flash_index = g_history_manager.cache[cache_index].flash_index;
    uint32_t flash_addr  = HISTORY_FLASH_START_ADDR + (flash_index * sizeof(store_History));

    NRF_LOG_DEBUG("Flash addr calculada: 0x%x, flash_index: %d, tamaño: %d", flash_addr,
                  flash_index, sizeof(store_History));

    // Validaciones exhaustivas
    if (flash_addr < HISTORY_FLASH_START_ADDR)
    {
        NRF_LOG_ERROR("Dirección flash menor al inicio: 0x%x < 0x%x", flash_addr,
                      HISTORY_FLASH_START_ADDR);
        return NRF_ERROR_INVALID_ADDR;
    }

    if (flash_addr + sizeof(store_History) > HISTORY_FLASH_END_ADDR)
    {
        NRF_LOG_ERROR("Dirección flash excede el final: 0x%x + %d > 0x%x", flash_addr,
                      sizeof(store_History), HISTORY_FLASH_END_ADDR);
        return NRF_ERROR_NO_MEM;
    }

    // Verificar alineación (debe ser múltiplo de 4)
    if (flash_addr % 4 != 0)
    {
        NRF_LOG_ERROR("Dirección flash no alineada: 0x%x", flash_addr);
        return NRF_ERROR_INVALID_ADDR;
    }

    if (sizeof(store_History) % 4 != 0)
    {
        NRF_LOG_ERROR("Tamaño de estructura no alineado: %d", sizeof(store_History));
        return NRF_ERROR_INVALID_LENGTH;
    }

    // Verificar si necesitamos borrar la página
    uint32_t page_addr      = (flash_addr / HISTORY_PAGE_SIZE) * HISTORY_PAGE_SIZE;
    uint16_t record_in_page = (flash_addr - page_addr) / sizeof(store_History);

    NRF_LOG_RAW_INFO("\n\nPage addr: 0x%x, record_in_page: %d", page_addr, record_in_page);

    if (record_in_page == 0)
    {
        NRF_LOG_RAW_INFO("\n\nBorrando pagina en direccion 0x%x", page_addr);
        ret_code_t rc = nrf_fstorage_erase(&history_fstorage, page_addr, 1, NULL);
        if (rc != NRF_SUCCESS)
        {
            NRF_LOG_ERROR("Error borrando pagina: %d en addr 0x%x", rc, page_addr);
            return rc;
        }

        NRF_LOG_DEBUG("Esperando que termine el borrado...");
        history_wait_for_flash_ready();
        NRF_LOG_DEBUG("Borrado completado");
    }

    NRF_LOG_RAW_INFO("\nEscribiendo %d bytes a flash en direccion 0x%x", sizeof(store_History),
                     flash_addr);

    ret_code_t rc = nrf_fstorage_write(&history_fstorage, flash_addr,
                                       &g_history_manager.cache[cache_index].record,
                                       sizeof(store_History), NULL);

    if (rc == NRF_SUCCESS)
    {
        NRF_LOG_DEBUG("Comando de escritura enviado, esperando completacion...");
        history_wait_for_flash_ready();
        g_history_manager.cache[cache_index].state = CACHE_CLEAN;
        NRF_LOG_DEBUG("Escritura completada exitosamente");
    }
    else
    {
        NRF_LOG_ERROR("Error en comando de escritura: %d", rc);
    }

    return rc;
}

static ret_code_t history_read_from_flash(uint16_t flash_index, store_History *record)
{
    if (flash_index >= MAX_HISTORY_RECORDS || record == NULL)
    {
        return NRF_ERROR_INVALID_PARAM;
    }

    uint32_t   flash_addr = HISTORY_FLASH_START_ADDR + (flash_index * sizeof(store_History));

    ret_code_t rc = nrf_fstorage_read(&history_fstorage, flash_addr, record, sizeof(store_History));

    return rc;
}

static ret_code_t history_erase_page(uint16_t page_number)
{
    uint32_t page_addr = HISTORY_FLASH_START_ADDR + (page_number * HISTORY_PAGE_SIZE);

    if (page_addr >= HISTORY_FLASH_END_ADDR)
    {
        return NRF_ERROR_INVALID_PARAM;
    }

    ret_code_t rc = nrf_fstorage_erase(&history_fstorage, page_addr, 1, NULL);

    if (rc == NRF_SUCCESS)
    {
        history_wait_for_flash_ready();
    }

    return rc;
}

static uint8_t history_find_cache_slot(uint16_t flash_index)
{
    for (uint8_t i = 0; i < HISTORY_CACHE_SIZE; i++)
    {
        if (g_history_manager.cache[i].state != CACHE_EMPTY &&
            g_history_manager.cache[i].flash_index == flash_index)
        {
            return i;
        }
    }
    return HISTORY_CACHE_SIZE; // No encontrado
}

static uint8_t history_get_free_cache_slot(void)
{
    // Buscar slot vacío primero
    for (uint8_t i = 0; i < HISTORY_CACHE_SIZE; i++)
    {
        if (g_history_manager.cache[i].state == CACHE_EMPTY)
        {
            NRF_LOG_DEBUG("Encontrado slot libre: %d", i);
            return i;
        }
    }

    // No hay slots vacíos
    NRF_LOG_DEBUG("No hay slots libres en cache");
    return HISTORY_CACHE_SIZE; // Indicar que no hay espacio
}

static void history_update_cache_lru(uint8_t cache_index)
{
    // Implementación simple de LRU: mover el elemento usado al final
    // En una implementación más sofisticada, podrías usar timestamps
    if (cache_index < HISTORY_CACHE_SIZE - 1)
    {
        history_cache_entry_t temp = g_history_manager.cache[cache_index];
        memmove(&g_history_manager.cache[cache_index], &g_history_manager.cache[cache_index + 1],
                (HISTORY_CACHE_SIZE - cache_index - 1) * sizeof(history_cache_entry_t));
        g_history_manager.cache[HISTORY_CACHE_SIZE - 1] = temp;
    }
}
