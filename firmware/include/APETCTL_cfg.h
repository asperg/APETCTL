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
/* Maximum allowed temperature [degree C] x 10, allowed to set to 10 degree less */
#define CFG_TEMP_MAX_X10 3300
/* Minimum allowed temperature to set [degree C] x 10 */
#define CFG_TEMP_MIN_X10 1200
/* Which pin termistor connected to*/
#define CFG_TERM_PIN A0
/* Which pin emergency endstop connected to */
#define CFG_EMENDSTOP_PIN 11

/* PID regulator coefficients */
#define CFG_PID_P 64000L
#define CFG_PID_I 100L
#define CFG_PID_D 0L
#define CFG_PID_I_LIMIT 2000000L

/* Which pin heater MOSFET connected to 
Из за особенностей кода может быть от 8 до 13 пина.
*/
#define CFG_HEATER_PIN 9

/* Энкодер длины протянутого прутка */
#define CFG_LENGHT_PIN A2  //A2

/* Initial pull speed [mm/s] */
#define CFG_SPEED_INIT 2

#endif