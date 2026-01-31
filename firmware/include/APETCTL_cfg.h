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
#define CFG_TEMP_MIN_X10 1200
/* Which pin termistor connected to*/
#define CFG_TERM_PIN A0
/* Which pin emergency endstop connected to */
#define CFG_EMENDSTOP_PIN 11

/* PID regulator coefficients */
#define CFG_PID_P 100000L
#define CFG_PID_I 333L
#define CFG_PID_D 700000L
#define CFG_PID_I_LIMIT 75000L // 33333000/CFG_PID_I

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

#endif