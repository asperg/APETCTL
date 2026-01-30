#ifndef APETCFG_H
#define APETCFG_H
/* Which pin stepper driver STEP pin connected 
из за особеннойстей кода STEP pin должен быть
в диапазоне от 0 до 7
*/
#define CFG_STEP_STEP_PIN 6
/* Which pin stepper driver DIR pin connected */
#define CFG_STEP_DIR_PIN 5
/* Which pin stepper driver EN pin connected */
#define CFG_STEP_EN_PIN 7

/* Which pin encoder CLK pin connected 
   Which pin encoder DT pin connected 
   HARDCODE Do Not Change this*/
#define CFG_ENC_CLK 2
#define CFG_ENC_DT 3
#define CFG_ENC_SW 4

/* Initial target temperature [degree C]*/
#define CFG_TEMP_INIT 250
/* Maximum allowed temperature [degree C], allowed to set to 10 degree less */
#define CFG_TEMP_MAX_X10 3300
/* Minimum allowed temperature to set [degree C] */
#define CFG_TEMP_MIN 120
/* Which pin termistor connected to*/
#define CFG_TERM_PIN A0
/* Which pin emergency endstop connected to */
#define CFG_EMENDSTOP_PIN 11

/* PID regulator coefficients */
//PID p: 12.69  PID i: 0.71 PID d: 57.11
#define CFG_PID_P 1000
#define CFG_PID_I 3
#define CFG_PID_D 1000

/* Which pin heater MOSFET connected to 
Из за особенностей кода может быть от 8 до 13 пина.
*/
#define CFG_HEATER_PIN 9

/* Энкодер длины протянутого прутка */
#define CFG_LENGHT_PIN A2  //A2
#define CFG_ENC_DIAM 10.4  // диаметр измерительного ролика в мм
#define CFG_ENC_IMP 12   // импульсов на один оборот энкодера

/* Initial pull speed [mm/s] */
#define CFG_SPEED_INIT 2

/* Interactive statuses */
#define CHANGE_NO 0
#define CHANGE_TEMPERATURE 1
#define CHANGE_SPEED 2


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