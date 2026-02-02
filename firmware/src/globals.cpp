#include "globals.h"

volatile uint32_t heater_pwm_threshold = 0;       // Предел времени для ШИМ нагревателя
volatile unsigned long lastTimeInterrupt = 0;     // Время предыдущего прерывания от энкодера длинны
volatile unsigned long FilamentTiks = 0;          // Сколько всего натикал энкодер
volatile bool newDataFlag = false;                // Флаг, что обновились скорость и метраж

long targetSpeedX10 = (float)CFG_SPEED_INIT * 10; // То, что мы выставили энкодером

volatile int8_t encDelta = 0;          // количество кликов энкодера интерфейса отрицательные влево

//Кольцевой буффер для длительности между прерываниями энкодера
unsigned long enc_event_duration[RING_BUFFER_SIZE];
uint8_t eed_idx = 0;
volatile unsigned long eed_sum = 0;

long curTempX10 = 0;                        // Текущая температура измеренная АЦП
long targetTemp10 = (long)CFG_TEMP_INIT*10; // Целевая температура
bool EndPetTapeFlag = false;
bool SlimStopFlag = true;
bool Heat = false;
bool runMotor = false;

// Битовые маски для управления двигателем и нагревателем в прерывании
// рссчитываются заранее в setup
uint8_t motor_bit;
uint8_t heater_bit;

// Энкодер Интерфейса
unsigned long encLastActivity = 0;
unsigned long encLastClickTime = 0;

InerfaceMode currentMode = InerfaceMode::IDLE;
