#include <Arduino.h>
#include <avr/wdt.h>
#include "PinChangeInterrupt.h"
#include "PETCTL_cfg.h"
#include "temp_table.h"
#define SPEED_MAX 9.9
#define SPEED_MIN 1.1
// Functions prototype

void debugTemp(long temp, int out);
long mmStoDeg(float mmS);
void emStop(int reason);
void motorCTL(long setSpeedX10);
void printHeaterStatus(boolean status);
void printMotorStatus(boolean status);
void printTapeStatus(boolean status);
void encRotationToValue (long* value, int inc, long minValue, long maxValue);
void printTargetTemp(long t);
void printCurrentTemp(long t);
void printSpeed(long s);
void interactiveSet();
boolean isInteractive();
long getTemp();
void LengthEvent(void);
void SplashScreen(void);

#define DRIVER_STEP_TIME 10  // меняем задержку на 10 мкс

#include "GyverOLED.h"
GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled;

#define CLK CFG_ENC_CLK
#define DT CFG_ENC_DT
#define SW CFG_ENC_SW
#include "GyverEncoder.h"
Encoder enc1(CLK, DT, SW);
//int value = 0;

volatile uint8_t heater_pwm = 0;              // Значение 0-255 (как в analogWrite)
uint32_t heater_timer_acc = 0;                // Аккумулятор микросекунд
volatile unsigned long lastTimeInterrupt = 0; // Время предыдущего прерывания
volatile unsigned long FilamentTiks = 0;      // Сколько всего натикал энкодер
volatile bool newDataFlag = false;            // Флаг, что данные обновились

long targetSpeedX10 = (float)CFG_SPEED_INIT * 10; // То, что мы выставили энкодером
long currentSpeedX10 = (float)SPEED_MIN * 10;    // Реальная скорость в данный момент
uint32_t accelTimer = 0;                         // Таймер для шага разгона
#define ACCEL_STEP_MS 50                         // Интервал изменения скорости (мс)


#define RING_BUFFER_SIZE 16
uint16_t adc_buffer[RING_BUFFER_SIZE]; // массив для хранения последних 16 значений
uint8_t adc_idx = 0;                   // текущий индекс в массиве
uint32_t adc_sum = 0;                  // текущая сумма всех значений в буфере

//Кольцевой буффер для длительности между прерываниями энкодера
unsigned long enc_event_duration[RING_BUFFER_SIZE];
uint8_t eed_idx = 0;
volatile unsigned long eed_sum = 0;

// Метров за один импульс
const float STEP_METERS = (float)CFG_ENC_DIAM*(float)0.0031415926/(float)CFG_ENC_IMP;
// Коэффициент для расчета скорости через micros()
const float SPEED_CONSTANT = STEP_METERS*(float)1000000000; 

// Termistor definition
int prevTempX10, curTempX10 = 0;
int targetTemp = CFG_TEMP_INIT;

#include "GyverPID.h"
GyverPID regulator(CFG_PID_P, CFG_PID_I, CFG_PID_D, 200);

bool Heat = false;
bool runMotor=false;

/* Interactive statuses */
#define CHANGE_NO 0
#define CHANGE_TEMPERATURE 1
#define CHANGE_SPEED 2
int whatToChange = CHANGE_NO;
unsigned long interactive = millis();

/* Emergency stop reasons */
#define OVERHEAT 1
#define THERMISTOR_ERROR 2

void setup() {
  wdt_enable(WDTO_4S);

#if defined(SERIAL_DEBUG_TEMP) || defined(SERIAL_DEBUG_STEPPER) || defined(SERIAL_DEBUG_TEMP_PID)
  Serial.begin(9600);
#endif //SERIAL_DEBUG_TEMP || SERIAL_DEBUG_STEPPER

#if defined(__LGT8F__)
  analogReadResolution(10);
#endif

  pinMode(CFG_EMENDSTOP_PIN, INPUT_PULLUP);
  pinMode(CFG_LENGHT_PIN, INPUT_PULLUP);
  // Настройка пинов управления двигателем
  pinMode(CFG_STEP_STEP_PIN, OUTPUT); // Пин 6
  pinMode(CFG_STEP_DIR_PIN, OUTPUT);  // Пин 5
  pinMode(CFG_STEP_EN_PIN, OUTPUT);   // Пин 7
  pinMode(CFG_HEATER_PIN, OUTPUT);    // Нагреватель

  digitalWrite(CFG_STEP_EN_PIN, HIGH);  // Выключаем мотор (активный LOW)
  digitalWrite(CFG_STEP_DIR_PIN, LOW);  // Направление по умолчанию

  // Настройка Таймеров
  noInterrupts();
  TCCR1A = 0;
  TCCR1B = _BV(WGM12) | _BV(CS11); // Режим CTC, предделитель 8 (0.5 мкс)
  OCR1A = pgm_read_word(&step_table[CFG_SPEED_INIT]); // Начальное значение
  TIMSK1 |= _BV(OCIE1A);           // Разрешить прерывание
  // Настройка Timer2: частота 1кГц (период 1мс)
  TCCR2A = _BV(WGM21);             // Режим CTC (сброс при совпадении)
  TCCR2B = _BV(CS22) | _BV(CS21) | _BV(CS20); // Предделитель 1024
  OCR2A = 15;                      // (16MHz / 1024 / 1000Hz) - 1 = 14.6 -> 15
  TIMSK2 |= _BV(OCIE2A);           // Разрешить прерывание по совпадению A
  interrupts();
    
  // подключение обработки прерывания по сигналу от датчика
  attachPinChangeInterrupt(digitalPinToPinChangeInterrupt(CFG_LENGHT_PIN), LengthEvent, RISING);

  oled.init();              // инициализация
  // ускорим вывод, ВЫЗЫВАТЬ ПОСЛЕ oled.init()!!!
  //Wire.setClock(400000L);   // макс. 800'000
  Wire.setClock(800000L);   // макс. 800'000
  oled.clear();

  enc1.setType(CFG_ENC_TYPE);
  enc1.setPinMode(LOW_PULL);

  // Заполнить кольцевой буффер АЦП текущим значением из АЦП
  // кольцевой буффер длительности событий заполнить 
  // максимальными значениями (~0UL) >> 5, скорость то 0
  uint16_t startAdc = analogRead(CFG_TERM_PIN);
  for (int i = 0; i < RING_BUFFER_SIZE; i++) {
    adc_buffer[i] = startAdc;
    enc_event_duration[i] = (~0UL) >> 5;
  }
  adc_sum = (uint32_t)startAdc * RING_BUFFER_SIZE;
  eed_sum = ((~0UL) >> 5) * RING_BUFFER_SIZE;

  SplashScreen();  
  regulator.setpoint = targetTemp;
  printSpeed(targetSpeedX10);
  printTargetTemp(targetTemp);
  heater_pwm = 0;
}

// обработчик
ISR(TIMER2_COMPA_vect) {
  enc1.tick();
}

// обработчик двигателя, шагаем двигателм здесь !!!!!
ISR(TIMER1_COMPA_vect) {
  // 1. Мотор (максимальный приоритет по времени)
  if (runMotor) {
    PIND |= digitalPinToBitMask(CFG_STEP_STEP_PIN);
    delayMicroseconds(DRIVER_STEP_TIME);
    PIND |= digitalPinToBitMask(CFG_STEP_STEP_PIN);
  }
  
  // При частоте 16 МГц и предделителе 8, таймер тикает с частотой 2 МГц
  // Это значит: 1 тик = 0.5 микросекунды.
  // Переводим тики в микросекунды
  heater_timer_acc += (OCR1A >> 1); 

  if (heater_timer_acc >= 33333) { 
    heater_timer_acc = 0;
    // Начало нового цикла 30 Гц
    if (heater_pwm > 0 && Heat) digitalWrite(CFG_HEATER_PIN, HIGH);
  } else {
    // Конец импульса ШИМ (программная отсечка)
    // 33333 / 255 = ~130 мкс на одну единицу ШИМ
    if (heater_timer_acc > (uint32_t)heater_pwm * 130) {
      digitalWrite(CFG_HEATER_PIN, LOW);
    }
  }
}

void loop() {
  wdt_reset();
  enc1.tick();

  long newTargetTemp = targetTemp;
  long newSpeedX10 = targetSpeedX10;
  static bool EndPetTapeFlag = false;
  const char spinner[] = {'-', '/', '|', '\\'};
  static uint8_t spinnerIdx = 0;
  static uint32_t spinnerTimer = 0;
    
  if (millis() - spinnerTimer >= 100) {
    spinnerTimer = millis();
    oled.setCursorXY(105, 47);
    oled.setScale(2);
    oled.print(spinner[spinnerIdx]);
    if (++spinnerIdx >= 4) spinnerIdx = 0;
  }

  // Если новых импульсов нет больше 3 секунд — считаем, что скорость 0
  // и плавно заполняем кольцевой буфер ULONG_MAX / 32
  if (micros() - lastTimeInterrupt > 3000000) {
    noInterrupts();
    eed_sum -= enc_event_duration[eed_idx];
    // (~0UL) >> 5 = ULONG_MAX / 32
    enc_event_duration[eed_idx] = (~0UL) >> 5;
    eed_idx++;
    eed_sum += (~0UL) >> 5;
    if (eed_idx >= RING_BUFFER_SIZE) eed_idx = 0;
    newDataFlag = true; // Обновим экран, чтобы показать ноль
    interrupts();
  }

  // Обновляем экран только когда есть новые данные и в свободное время
  if (newDataFlag) {
    unsigned long copyEncoderEventDurationSum;
    unsigned long copyFilamentTiks;
    // Копируем данные во временные переменные при выключенных прерываниях
    noInterrupts();
    copyEncoderEventDurationSum = eed_sum;
    copyFilamentTiks = FilamentTiks;
    newDataFlag = false;
    interrupts();

    unsigned long avgDuration = copyEncoderEventDurationSum >> 4;
    //Считаем скорость: дистанция / время
    float CurrentFilamentSpeed;
    if(avgDuration == (~0UL) >> 5) {
      CurrentFilamentSpeed = 0.0;
    } else if(avgDuration != 0) {
      CurrentFilamentSpeed = SPEED_CONSTANT / (float)avgDuration;
    } else {
      CurrentFilamentSpeed = 0.0;
    }
    float FilamentLength = (float)copyFilamentTiks*STEP_METERS;
    // Вывести Метраж, мусора не будет т.к. только нарастает :)
    oled.setScale(2);
    oled.setCursorXY(12, 47);
    oled.print(FilamentLength, 3);  
    // вывести реальную (не расчетную скорость)
    oled.setCursorXY(12, 26);
    oled.print(CurrentFilamentSpeed, 1);
    //oled.print("   ");
  }

  if (runMotor) {
    if (millis() - accelTimer > ACCEL_STEP_MS) {
      accelTimer = millis();
    
      if (currentSpeedX10 < targetSpeedX10) {
        currentSpeedX10++; // Плавно ускоряем
        motorCTL(currentSpeedX10);
      } else if (currentSpeedX10 > targetSpeedX10) {
        currentSpeedX10--; // Плавно замедляем (если крутанули энкодер вниз)
        motorCTL(currentSpeedX10);
      }
    }
  }

  if (enc1.isDouble()) {
    whatToChange = CHANGE_SPEED;
    interactiveSet();
    printTargetTemp(targetTemp); // to clear selection
    printSpeed(targetSpeedX10);
  }
  if (enc1.isSingle()) {
    whatToChange = CHANGE_TEMPERATURE;
    interactiveSet();
    printSpeed(targetSpeedX10); // to clear selection
    printTargetTemp(targetTemp);
  }
  if (!isInteractive()) {
    whatToChange = CHANGE_NO;
    printSpeed(targetSpeedX10); // to clear selection
    printTargetTemp(targetTemp);
  }

  if( whatToChange == CHANGE_TEMPERATURE) {
    encRotationToValue(&newTargetTemp, 1, CFG_TEMP_MIN, CFG_TEMP_MAX_X10/10 - 10);
    if (enc1.isHolded()){
      Heat = ! Heat;
      printHeaterStatus(Heat);
    }

    if (newTargetTemp != targetTemp) {
      targetTemp = newTargetTemp;
      regulator.setpoint = newTargetTemp;
      printTargetTemp(newTargetTemp);
    }
  } else if (whatToChange == CHANGE_SPEED) {
    encRotationToValue(&newSpeedX10, 1, SPEED_MIN * 10, SPEED_MAX * 10);
    if (enc1.isHolded()) {
      runMotor = ! runMotor;
      interactiveSet();
      printMotorStatus(runMotor);
    }
    if (newSpeedX10 != targetSpeedX10) {
      targetSpeedX10 = newSpeedX10;
      printSpeed(targetSpeedX10);
    }
  }

  curTempX10 = getTemp();
  if (curTempX10 > CFG_TEMP_MAX_X10 - 100) emStop(OVERHEAT);
  regulator.input = (float)curTempX10/10.0;
  if (curTempX10 != prevTempX10) {
    prevTempX10 = curTempX10;
    printCurrentTemp(curTempX10);
  }
  int pidOut = regulator.getResultTimer();
  if (Heat) {
    heater_pwm = (uint8_t)constrain(pidOut, 0, 255);
    debugTemp(curTempX10, pidOut);
  } else {
    heater_pwm = 0;
    analogWrite(CFG_HEATER_PIN, 0);
    debugTemp(curTempX10, 0);
  }

  // Обработка датчика конца ПЭТ ленты
  // ререходим на машину состояний
  // Если произошло срабоатывание датчика
  if(!digitalRead(CFG_EMENDSTOP_PIN) && !EndPetTapeFlag) {
    EndPetTapeFlag = true;
    if(runMotor || Heat) {
      runMotor = false;
      Heat = false;
      heater_pwm = 0;
      digitalWrite(CFG_HEATER_PIN, LOW);   // Гасим нагрев немедленно
      digitalWrite(CFG_STEP_EN_PIN, HIGH); // Снимаем ток с мотора (свободное вращение)
      printHeaterStatus(Heat);
      printMotorStatus(runMotor);
    }
    printTapeStatus(EndPetTapeFlag);
  // если произошло отпускание датчика
  } else if(digitalRead(CFG_EMENDSTOP_PIN) && EndPetTapeFlag) {
    EndPetTapeFlag = false;
    printTapeStatus(EndPetTapeFlag);
  } 
}

// Обработка прерывания от датчика длины прутка
void LengthEvent(void) {
  // debug code
  digitalWrite(13, !digitalRead(13));

  unsigned long currentTime = micros();
  unsigned long duration = currentTime - lastTimeInterrupt;

  if (duration > 0) {
    eed_sum -= enc_event_duration[eed_idx];
    enc_event_duration[eed_idx] = duration;
    eed_sum += duration;
    eed_idx++;
    if (eed_idx >= RING_BUFFER_SIZE) eed_idx = 0;
    //unsigned long avgDuration = eed_sum >> 4;
    //Считаем скорость: дистанция / время
    //вынесу рассчет в модул вывода на экран
    //CurrentFilamentSpeed = SPEED_CONSTANT / (float)avgDuration;
    FilamentTiks++;
    lastTimeInterrupt = currentTime;
    newDataFlag = true; // Сообщаем основному циклу, что надо обновить экран
  }
}


void debugTemp(long temp, int out) {
#if defined(SERIAL_DEBUG_TEMP)
    static long debug_time;
    if (debug_time < millis() ) {
      debug_time = millis() + 1000;
      Serial.print(temp);
#if defined(SERIAL_DEBUG_TEMP_PID)
      Serial.print(' ');
      Serial.print(out);
#endif // end SERIAL_DEBUG_TEMP_PID
      Serial.println(' ');
    }
#endif //end SERIAL_DEBUG_TEMP
}

void emStop(int reason) {
  runMotor = false;
  Heat = false;
  heater_pwm = 0;
  digitalWrite(CFG_STEP_EN_PIN, HIGH); // Снять ток с мотора
  analogWrite(CFG_HEATER_PIN, 0);
  oled.clear();
  oled.setScale(3);
  oled.setCursorXY(0,2);
  oled.println("*HALT!*");
  oled.setScale(2);
  oled.setCursorXY(3,40);
  switch (reason) {
    case OVERHEAT:
      oled.println("Overheat");
      break;
    case THERMISTOR_ERROR:
      oled.println("Thermistor");
      break;
  }
  noInterrupts();
  TIMSK1 = 0; // Отключаем прерывания Таймера 1 (мотор и нагрев)
  TIMSK2 = 0; // Отключаем Таймер 2 
  digitalWrite(CFG_STEP_EN_PIN, HIGH); // Снять ток с мотора
  analogWrite(CFG_HEATER_PIN, 0);      // Отлключить нагреватель
  for(;;){
    delay(60000);
  }
}
 
void motorCTL(long setSpeedX10) {
  //Проверка на наизкую скорость, минимальная скорость 1 мм в сек
  if (runMotor && setSpeedX10 >= 10 && setSpeedX10 < 100) {
    if (digitalRead(CFG_STEP_EN_PIN) == HIGH) {
      digitalWrite(CFG_STEP_EN_PIN, LOW);
      delay(1); 
    }
    //На основе значений из таблицы установить частоту обновления таймера
    int index = constrain(setSpeedX10, 0, 99);
    uint16_t period = pgm_read_word(&step_table[index]);
    noInterrupts();
    OCR1A = period;
    // ЕСЛИ новый период меньше текущего счетчика, сбрасываем счетчик,
    // чтобы мотор не ждал полного круга таймера в 65535 тиков
    if (TCNT1 >= period) TCNT1 = 0; 
    interrupts();
  } else {
    runMotor = false;
    if (digitalRead(CFG_STEP_EN_PIN) == LOW) {
      digitalWrite(CFG_STEP_EN_PIN, HIGH); // Выключаем удержание
    }
  }
}

void printHeaterStatus(boolean status) {
  oled.setCursorXY(0, 0);
  oled.setScale(2);
  if(status) 
    oled.print("*");
  else
    oled.print(".");
}

void printMotorStatus(bool status) {
  oled.setCursorXY(0, 23);
  oled.setScale(2);
  if(status) 
    oled.print("*");
  else
    oled.print(".");
}

void printTapeStatus(bool status) {
  oled.setCursorXY(0, 47);
  oled.setScale(2);
  if(status) 
    oled.print("X");
  else
    oled.print(" ");
}


void encRotationToValue (long* value, int inc = 1, long minValue = 0, long maxValue = 0) {
      if (enc1.isRight()) { *value += inc; interactiveSet(); }     // если был поворот направо, увеличиваем на 1
      if (enc1.isFastR()) { *value += inc * 5; interactiveSet(); }    // если был быстрый поворот направо, увеличиваем на 10
      if (enc1.isLeft())  { *value -= inc; interactiveSet(); }     // если был поворот налево, уменьшаем на 1
      if (enc1.isFastL()) { *value -= inc * 5; interactiveSet(); }    // если был быстрый поворот налево, уменьшаем на на 10
      //if (minValue > 0 && *value < minValue) *value = minValue;
      if (*value < minValue) *value = minValue;
      //if (maxValue > 0 && *value > maxValue) *value = maxValue;
      if (*value > maxValue) *value = maxValue;
}

void printTargetTemp(long t){
      oled.setScale(2);      
      if(whatToChange == CHANGE_TEMPERATURE)  oled.invertText(true);
      oled.setCursorXY(88, 0);
      oled.println(t, 1);  
      oled.invertText(false);
}

//Входной параметр температура X10
void printCurrentTemp(long t) {
      oled.setScale(2);      
      oled.setCursorXY(12, 0);
      if (t < 1000) oled.print(" ");
      if (t < 100) oled.print(" ");
      oled.print( t / 10 );
      oled.print( ".");
      oled.print( t % 10 );
}

void printSpeed(long s){
      // s -speed in mm/s * 10
      // // pint in mm/s
      oled.setScale(2);      
      oled.setCursorXY(84, 23);
      if(whatToChange == CHANGE_SPEED)  oled.invertText(true);
      oled.print((float)s/10, 1);
//      if (s<100) oled.print(" "); //fix display garbage 
      oled.invertText(false);
}

void interactiveSet() {
  interactive = millis() + 15000;
}

boolean isInteractive() {
  return millis() < interactive;
}

long getTemp() {
  uint16_t raw = analogRead(CFG_TERM_PIN);
  // после переключения мультиплексора на нужный пин
  // дать небольшой таймаут для выравниваия потенциала
  delayMicroseconds(13);          
  raw = analogRead(CFG_TERM_PIN);

  // 2. Алгоритм скользящего среднего
  adc_sum -= adc_buffer[adc_idx]; // Вычитаем самое старое значение из суммы
  adc_buffer[adc_idx] = raw;      // Записываем новое значение на его место
  adc_sum += raw;                 // Добавляем новое значение к общей сумме

  // Инкремент индекса (с возвратом в 0 при достижении 16)
  adc_idx++;
  if (adc_idx >= RING_BUFFER_SIZE) adc_idx = 0;
  // 3. Вычисляем среднее АЦП
  // Вместо деления на 16 используем сдвиг вправо на 4 бита
  uint16_t avgAdc = adc_sum >> 4;

  // получить температуру из таблицы
  //
  int t = (int)pgm_read_word(&tempTable[avgAdc]);
  if (t == -1) {
    emStop(THERMISTOR_ERROR);
  }

  // Возвращаем среднее значение температуры умноженное на 10
  return (long)t;
}

void SplashScreen(void) {
  oled.setScale(3);
  oled.setCursor(5, 2);
  oled.println("APETctl");
  oled.setScale(1);
  oled.setCursor(20, 7);
  oled.print("asper V 0.4");
  delay(2500);
 
  oled.clear();
  oled.setScale(1);
  oled.setCursorXY(74,5);
  oled.print("*C");
  oled.setCursorXY(55,5+5+16);
  oled.print("mm/s");
  oled.setCursorXY(98,5+5+5+5+32);
  oled.print("m");
}