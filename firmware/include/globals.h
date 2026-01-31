#ifndef GLOBALS_H
#define GLOBALS_H
#include <Arduino.h> 
#include "APETCTL_cfg.h"

#define TOGGLE_LED (PINB = (1 << 5))              //　Переключить встроенный светодиод
#define SET_LED_OUTPUT (DDRB |= (1 << 5))         // Перевести PIN 13 в режим OUTPUT получится яркий светодиод
#define RING_BUFFER_SIZE 16
#define SPEED_MAX10 99
#define SPEED_MIN10 5
#define OVERHEAT 1
#define THERMISTOR_ERROR 2
#define ENC_MASK_CLK (1 << 2)
#define ENC_MASK_DT (1 << 3)
#define ENC_MASK_SW (1 << 4)


extern volatile uint32_t heater_pwm_threshold;
extern volatile unsigned long lastTimeInterrupt;
extern volatile unsigned long FilamentTiks;
extern volatile bool newDataFlag;
extern long targetSpeedX10;
extern volatile int8_t deltaTemp;  
extern volatile int8_t deltaSpeed;
extern const int8_t encTable[];
extern uint16_t adc_buffer[];
extern uint8_t adc_idx;
extern uint32_t adc_sum;
extern unsigned long enc_event_duration[];
extern uint8_t eed_idx;
extern volatile unsigned long eed_sum;
extern long curTempX10;
extern long targetTemp10;
extern bool EndPetTapeFlag;
extern bool SlimStopFlag;
extern bool Heat;
extern bool runMotor;
extern uint8_t motor_bit;
extern uint8_t heater_bit;
extern unsigned long encLastActivity;
extern unsigned long encLastClickTime;
extern int encClickCount;

enum class InerfaceMode { IDLE, EDIT_TEMP, EDIT_SPEED };
extern InerfaceMode currentMode;

// Таблица расчета длительности таймера 1, индекс таблицы
// скорость в мм в сек умноженнгая на 10
// значение: задержка между шагами двигателя в тиках таймера
// при следующих параметрах 1 тик 0.5 мкс
// передаточное отношение редуктора 1:74.4, 
// микрошаг 8, диаметр бобины 90мм
// так как таймер 16-битный, то минимальная скорость получилась 0.9мм в сек
// максимальную я ограничил на 9.9 мм/сек при этом во всем диапазоне
// скорость будет меняться плавно !!!!!
const uint16_t step_table[] PROGMEM = {
  0,    47534, 23767, 15845, 11884, 9507, 7922, 6791, 5942, 5282, // 0.0 - 0.9
  4753,  4321,  3961,  3656,  3395, 3169, 2971, 2796, 2641, 2502, // 1.0 - 1.9
  2377,  2264,  2161,  2067,  1981, 1901, 1828, 1761, 1698, 1639, // 2.0 - 2.9
  1584,  1533,  1485,  1440,  1398, 1358, 1320, 1285, 1251, 1219, // 3.0 - 3.9
  1188,  1159,  1132,  1105,  1080, 1056, 1033, 1011,  990,  970, // 4.0 - 4.9
  951,    932,   914,   897,   880,  864,  849,  834,  820,  806, // 5.0 - 5.9
  792,    779,   767,   755,   743,  731,  720,  709,  699,  689, // 6.0 - 6.9
  679,    670,   660,   651,   642,  634,  625,  617,  609,  602, // 7.0 - 7.9
  594,    587,   580,   573,   566,  559,  553,  546,  540,  534, // 8.0 - 8.9
  528,    522,   517,   511,   506,  500,  495,  490,  485,  480  // 9.0 - 9.9
};

#endif
