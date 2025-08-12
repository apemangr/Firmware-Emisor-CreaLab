#ifndef __HISTORY_MANAGER_H
#define __HISTORY_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include "nrf_fstorage.h"
#include "Variables.h"

// Configuración del gestor de historial
#define HISTORY_FLASH_START_ADDR    0x70000    // Dirección más baja y segura
#define HISTORY_FLASH_END_ADDR      0x75FFF    // ~24KB para historiales  
#define HISTORY_PAGE_SIZE           4096       // Tamaño de página flash
#define HISTORY_RECORDS_PER_PAGE    (HISTORY_PAGE_SIZE / sizeof(store_History))
#define MAX_HISTORY_RECORDS         250        // Mantener el límite actual
#define HISTORY_CACHE_SIZE          5          // Solo 5 registros en RAM

// Estados del caché
typedef enum {
    CACHE_EMPTY = 0,
    CACHE_DIRTY,    // Necesita escribirse a flash
    CACHE_CLEAN     // Sincronizado con flash
} cache_state_t;

// Entrada del caché
typedef struct {
    store_History record;
    cache_state_t state;
    uint16_t flash_index;  // Índice en flash
} history_cache_entry_t;

// Gestor de historial
typedef struct {
    history_cache_entry_t cache[HISTORY_CACHE_SIZE];
    uint16_t current_cache_size;
    uint16_t total_records;        // Total de registros guardados
    uint16_t next_flash_index;     // Próximo índice a escribir en flash
    uint16_t current_page;         // Página actual en uso
    bool circular_mode;            // Si ya llegamos al límite y empezamos a sobrescribir
} history_manager_t;

// Instancia global del gestor
extern history_manager_t g_history_manager;

// Funciones públicas
ret_code_t history_manager_init(void);
ret_code_t history_add_record(const store_History* record);
ret_code_t history_get_record(uint16_t index, store_History* record);
ret_code_t history_get_last_record(store_History* record);
ret_code_t history_clear_all(void);
ret_code_t history_flush_cache(void);
uint16_t history_get_total_count(void);
ret_code_t history_get_record_range(uint16_t start_index, uint16_t count, store_History* records);

// Funciones internas
static ret_code_t history_write_cache_to_flash(uint8_t cache_index);
static ret_code_t history_read_from_flash(uint16_t flash_index, store_History* record);
static ret_code_t history_erase_page(uint16_t page_number);
static uint8_t history_find_cache_slot(uint16_t flash_index);
static uint8_t history_get_free_cache_slot(void);
static void history_update_cache_lru(uint8_t cache_index);

#endif // __HISTORY_MANAGER_H
