#include "globals.h"

volatile uint32_t heater_pwm_threshold = 0;       // Предел времени для ШИМ нагревателя
uint32_t heater_timer_acc = 0;                    // Аккумулятор микросекунд для ШИМ нагревателя
volatile unsigned long lastTimeInterrupt = 0;     // Время предыдущего прерывания от энкодера длинны
volatile unsigned long FilamentTiks = 0;          // Сколько всего натикал энкодер
volatile bool newDataFlag = false;                // Флаг, что обновились скорость и метраж

long targetSpeedX10 = (float)CFG_SPEED_INIT * 10; // То, что мы выставили энкодером
long currentSpeedX10 = SPEED_MIN10;               // Реальная скорость в данный момент
uint32_t accelTimer = 0;                          // Таймер для шага разгона

// Переменные для обработки энкодера интерфейса
int8_t subStep = 0; // Накопитель для 4-х фаз щелчка
// чтобы не останавливать прерывания на время работы
// с данными полученными в прерывании буду работать с
// дельта буфером !!!
volatile int8_t deltaTemp = 0;  
volatile int8_t deltaSpeed = 0;
// Таблица переходов (State Machine Table)
// 0: нет движения, 1: вправо, -1: влево
const int8_t encTable[] = {0, 1, -1, 0, -1, 0, 0, 1, 1, 0, 0, -1, 0, -1, 1, 0};


uint16_t adc_buffer[RING_BUFFER_SIZE]; // массив для хранения последних 16 значений АЦП
uint8_t adc_idx = 0;                   // текущий индекс в массиве
uint32_t adc_sum = 0;                  // текущая сумма всех значений в буфере

//Кольцевой буффер для длительности между прерываниями энкодера
unsigned long enc_event_duration[RING_BUFFER_SIZE];
uint8_t eed_idx = 0;
volatile unsigned long eed_sum = 0;

// Метров за один импульс
const float STEP_METERS = (float)CFG_ENC_DIAM*(float)0.0031415926/(float)CFG_ENC_IMP;
// Коэффициент для расчета текущей скорости ленты 
const float SPEED_CONSTANT = STEP_METERS*(float)1000000000; 


long curTempX10 = 0;                        // Текущая температура измеренная АЦП
long targetTemp10 = (long)CFG_TEMP_INIT*10; // Целевая температура
// for pid regulator
long pid_integral = 0;
long pid_lastError = 0;
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
int encClickCount = 0;

InerfaceMode currentMode = InerfaceMode::IDLE;
