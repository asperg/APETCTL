#ifndef GLOBALS_H
#define GLOBALS_H
#include <Arduino.h> 
#include "APETCTL_cfg.h"

#define TOGGLE_LED (PINB = (1 << 5))              //　Переключить встроенный светодиод
#define SET_LED_OUTPUT (DDRB |= (1 << 5))         // Перевести PIN 13 в режим OUTPUT получится яркий светодиод
#define ACCEL_STEP_MS 50                          // Интервал изменения скорости (мс)
#define RING_BUFFER_SIZE 16
#define SPEED_MAX10 99
#define SPEED_MIN10 11
#define OVERHEAT 1
#define THERMISTOR_ERROR 2
#define ENC_MASK_CLK (1 << 2)
#define ENC_MASK_DT (1 << 3)
#define ENC_MASK_SW (1 << 4)


extern volatile uint32_t heater_pwm_threshold;
extern uint32_t heater_timer_acc;
extern volatile unsigned long lastTimeInterrupt;
extern volatile unsigned long FilamentTiks;
extern volatile bool newDataFlag;
extern long targetSpeedX10;
extern long currentSpeedX10;
extern uint32_t accelTimer;
extern int8_t subStep;
extern volatile int8_t deltaTemp;  
extern volatile int8_t deltaSpeed;
extern const int8_t encTable[];
extern uint16_t adc_buffer[];
extern uint8_t adc_idx;
extern uint32_t adc_sum;
extern unsigned long enc_event_duration[];
extern uint8_t eed_idx;
extern volatile unsigned long eed_sum;
extern const float STEP_METERS;
extern const float SPEED_CONSTANT; 
extern long curTempX10;
extern long targetTemp10;
extern long pid_integral;
extern long pid_lastError;
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
#endif
