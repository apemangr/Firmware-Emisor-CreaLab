#include "Variables.h"

// Definiciones de todas las variables globales
// (Las declaraciones están en Variables.h como extern)

uint8_t m_beacon_info[Largo_Advertising];
uint32_t contador = 0x00;
int8_t Potencia_antenna = -20; // Radio transmit power in dBm
int16_t Max_Time_Unconfident = 400; // Tiempo maximo de conexion establecida

bool device_sleep = false;
uint32_t sleep_in_time_ticker = 0x6000;
uint32_t APP_ADV_DURATION = 1500;

uint32_t Counter_sleep_ticker = 0x0000;
bool Wakeup_by_Timer = false;
const nrf_drv_rtc_t rtc = NRF_DRV_RTC_INSTANCE(2); // Declaring an instance of nrf_drv_rtc for RTC0.
uint16_t loop_timer = 1;

bool Device_on = true;
bool Wakeup_by_button = false;
uint16_t loop_button = 1;

bool Device_BLE_connected = false;
bool connection_confident = false;
int16_t Counter_to_disconnect = 0x00;

bool Sensor_connected = false;
bool Boton_Presionado = false;
int16_t Time_button_pressed = 0x00;
int16_t MAX_Time_Button_pressed = 20;

bool Transmiting_Ble = false;
uint16_t Counter_Time_blink = 0x00;
int8_t Blink_led_Time_on = 1;
int8_t Blink_led_Time_off = 7;
bool Led_encendido = false;

float Lenght_Cut = 0.0f;
float Step_of_resistor = 2.0f;
float Resistor_Cuted = 0.0f;

uint16_t Battery_level = 0;

int32_t temp = 0;

uint8_t data_flash[SIZE_FLASH] = {0};
uint32_t len = SIZE_FLASH;

bool Write_Flash = false;
bool Write_DATE = false;
uint16_t Timer_write_Date = 600;
uint16_t Counter_Timer_write_Date = 0;

uint8_t Tipo_dispositivo = 0x10;
uint8_t Tipo_Configuracion_ohm = 0x00;
uint8_t Offset_desgaste = 1;
int8_t Offset_sensor = 0x00;

uint8_t Reset_Index = 0;
uint8_t Reset_Line = 0x00;

bool Rtc_waiting = false;
uint8_t rtc_time = 0;

bool Another_Value = false;

uint16_t Valor_1 = 0;
uint16_t Valor_2 = 0;

uint16_t loop_send_med = 0;
int8_t Tipo_Envio = 0;
int8_t Configuration = 1;
int8_t History = 2;
int8_t Last_History = 3;
int8_t History_By_Index = 4;
int8_t Values = 5;

bool Next_Sending = false;

bool impresion_log = false;

int8_t Sensibilidad_Res = 0;

uint16_t History_Position = 0;
uint16_t Sending_Position = 0;
store_History History_value = {0};

store_flash Flash_array = {0};
store_flash Flash_Factory = {0};
