#ifndef __HISTORY_ADAPTER_H
#define __HISTORY_ADAPTER_H

#include "Variables.h"
#include "History_Manager.h"

// Funciones para mantener compatibilidad con el código existente

// Reemplaza: Flash_array.history[index]
ret_code_t get_history_record(uint16_t index, store_History* record);

// Reemplaza: Flash_array.history[History_Position] = new_record
ret_code_t set_history_record(uint16_t index, const store_History* record);

// Reemplaza: Flash_array.last_history
uint16_t get_last_history_index(void);

// Reemplaza: Flash_array.last_history = value
void set_last_history_index(uint16_t index);

// Obtiene el último registro válido
ret_code_t get_last_history_record(store_History* record);

// Función para migrar datos existentes (si es necesario)
ret_code_t migrate_existing_history_data(void);

// Función para limpiar todo el historial
ret_code_t clear_all_history(void);

// Función para obtener el total de registros
uint16_t get_total_history_count(void);

// Función de diagnóstico para verificar sincronización
void debug_print_history_sync_status(void);

#endif // __HISTORY_ADAPTER_H
