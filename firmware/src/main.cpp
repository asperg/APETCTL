#include <Arduino.h>
#include <avr/wdt.h>
#include "PinChangeInterrupt.h"
#include "temp_table.h"
#include "functions.h"
#include "oled.h"
#include "globals.h"


void setup() {
  wdt_enable(WDTO_4S);

  pinMode(CFG_EMENDSTOP_PIN, INPUT_PULLUP);  // Концевик конца или обрыва ленты
  pinMode(CFG_LENGHT_PIN, INPUT_PULLUP);     // Энкодер - измерение длины 
  pinMode(CFG_ENC_CLK, INPUT_PULLUP);        // Энкодер - управление интерфейсом
  pinMode(CFG_ENC_DT, INPUT_PULLUP);         //   -//-
  pinMode(CFG_ENC_SW, INPUT_PULLUP);         //   -//-
  // Настройка пинов управления двигателем
  pinMode(CFG_STEP_STEP_PIN, OUTPUT); // Пин 6
  pinMode(CFG_STEP_DIR_PIN, OUTPUT);  // Пин 5
  pinMode(CFG_STEP_EN_PIN, OUTPUT);   // Пин 7
  pinMode(CFG_HEATER_PIN, OUTPUT);    // Нагреватель

  // Заранее вычислить битовые маски для пинов
  // шаг двигателем и нагревателя
  motor_bit = digitalPinToBitMask(CFG_STEP_STEP_PIN);
  heater_bit = digitalPinToBitMask(CFG_HEATER_PIN);

  digitalWrite(CFG_STEP_EN_PIN, HIGH);  // Выключаем мотор (активный LOW)
  digitalWrite(CFG_STEP_DIR_PIN, LOW);  // Направление по умолчанию
  PORTD &= ~motor_bit;                  // Перевести пин STEP в 0

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
    
  // подключение обработки прерывания по сигналу от энкодера ленты
  attachPinChangeInterrupt(digitalPinToPinChangeInterrupt(CFG_LENGHT_PIN), LengthEventISR, RISING);

  oled_init();
  oled_clear();
  SplashScreen();

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

  // инициализация прерываний энкодера
  attachInterrupt(0, encoderISR, CHANGE); // Пин 2
  attachInterrupt(1, encoderISR, CHANGE); // Пин 3

  printTargetSpeed();
  printTargetTemp();
  printHeaterStatus();
  printMotorStatus();
  printTapeStatus();
}

// обработчики прерываний
// обработчик двигателя, шагаем двигателм здесь !!!!!
ISR(TIMER1_COMPA_vect) {
  // 1. Мотор (максимальный приоритет по времени)
  if (runMotor) {
    PORTD |= motor_bit;  // Гарантированный HIGH
    // Вместо delayMicroseconds(1)
    asm volatile("nop"); // Пауза в 1 такт (62.5 наносекунды)
    asm volatile("nop"); 
    PORTD &= ~motor_bit; // Гарантированный LOW
  }
  
  // При частоте 16 МГц и предделителе 8, таймер тикает с частотой 2 МГц
  // Это значит: 1 тик = 0.5 микросекунды.
  // Переводим тики в микросекунды
  heater_timer_acc += (OCR1A >> 1); 
  if (heater_timer_acc >= 33333) { 
    heater_timer_acc = 0;
    // Начало нового цикла 30 Гц
    if (heater_pwm_threshold > 0 && Heat) PORTB |= heater_bit;
  } else {
    // Конец импульса ШИМ (программная отсечка)
    // 33333 / 255 = ~130 мкс на одну единицу ШИМ
    if (heater_timer_acc > heater_pwm_threshold) {
      PORTB &= ~heater_bit;
    }
  }
}

// обработчик энкодера интерфейса
void encoderISR() {
  static uint8_t state = 0;
  static unsigned long lastStep = 0;

  uint8_t currentState = (PIND >> 2) & 0x03;
  state = (state << 2) | currentState;
  int8_t res = encTable[state & 0x0F]; // оставить только младшие 4 бита

  // ничего не считать если интерфейс в состоянии простоя (отображения)
  if (res != 0 && currentMode != InerfaceMode::IDLE) {
    subStep += res;
    if (abs(subStep) >= 4) { // Когда прошли все 4 фазы щелчка

      unsigned long now = millis();
      int8_t step = (now - lastStep < 50) ? 5 : 1;
      int8_t dir = (subStep > 0) ? 1 : -1;

      if (currentMode == InerfaceMode::EDIT_TEMP) deltaTemp += dir * step;
      else if (currentMode == InerfaceMode::EDIT_SPEED) deltaSpeed += dir * step;

      lastStep = now;
      subStep = 0; // Сброс накопителя
    }
  }
}

// Обработка прерывания от датчика длины прутка
void LengthEventISR(void) {
  // debug code
  //digitalWrite(13, !digitalRead(13));
  TOGGLE_LED;

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


void loop() {
  wdt_reset();

  const char spinner[] = {'-', '\\', '|', '/'};
  static uint8_t spinnerIdx = 0;
  static uint32_t spinnerTimer = 0;

// SPINNER ----------------------------------------------------    
// 10  раз в секунду
  if (millis() - spinnerTimer >= 100) {
    spinnerTimer = millis();
    oled_printChar(120, 7, spinner[spinnerIdx], false);
    // В двое медленне, здесь опрос температуры !!! и расчет пида
    if (spinnerIdx % 2) {
      // TEMPERATURE ---------------------------
      curTempX10 = getTemp();
      if (curTempX10 > CFG_TEMP_MAX_X10 - 100) emStop(OVERHEAT);
      if (Heat) {
        uint32_t new_heater_pwm_thresold = 130 * (uint32_t)computePID();
        // ШИМ 30 герц, таймер 2МГц итого 130 микросекунт на один уровень ШИМ
        noInterrupts();
        heater_pwm_threshold = new_heater_pwm_thresold;
        interrupts();
        }
      else {
        noInterrupts();
        heater_pwm_threshold = 0;
        interrupts();
      }
      printCurrentTemp();
    }
    if (++spinnerIdx >= 4) spinnerIdx = 0;
  }

  // Если новых импульсов нет больше 3 секунд — считаем, что скорость 0
  // флаг указывает на то что ранее измеренная скорость не равна 0
  // и плавно заполняем кольцевой буфер ULONG_MAX / 32
  if (SlimStopFlag && micros() - lastTimeInterrupt > 3000000) {
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
      // Отключить проверку на наличие прерываний
      // для плавного уменьшения показателя скорости на экране
      // т.к. скорость уже 0
      SlimStopFlag = false;
    } else if(avgDuration != 0) {
      CurrentFilamentSpeed = SPEED_CONSTANT / (float)avgDuration;
      SlimStopFlag = true;
    } else {
      CurrentFilamentSpeed = 0.0;
      SlimStopFlag = false;
    }
    float FilamentLength = (float)copyFilamentTiks*STEP_METERS;
    printMillageAndSpeed(FilamentLength, CurrentFilamentSpeed);
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

  handleEncButton();

  if (deltaTemp != 0) {
    int8_t copyDelta = deltaTemp; // Копируем 1 байт (безопасно)
    deltaTemp = 0;                // Сбрасываем (безопасно)
    targetTemp10 = constrain(targetTemp10 + copyDelta*10, CFG_TEMP_MIN*10, CFG_TEMP_MAX_X10);
    encLastActivity = millis();
    printTargetTemp();
  }

  if (deltaSpeed != 0) {
    int8_t copyDelta = deltaSpeed;
    deltaSpeed = 0;
    targetSpeedX10 = constrain(targetSpeedX10 + copyDelta, SPEED_MIN10, SPEED_MAX10);
    encLastActivity = millis();
    printTargetSpeed();
  }

  // Обработка датчика конца ПЭТ ленты
  // ререходим на машину состояний
  // Если произошло срабоатывание датчика
  if(!digitalRead(CFG_EMENDSTOP_PIN) && !EndPetTapeFlag) {
    EndPetTapeFlag = true;
    if(runMotor || Heat) {
      runMotor = false;
      Heat = false;
      noInterrupts();
      heater_pwm_threshold = 0;
      interrupts();
      digitalWrite(CFG_STEP_EN_PIN, HIGH); // Снимаем ток с мотора (свободное вращение)
      printHeaterStatus();
      printMotorStatus();
    }
    printTapeStatus();
  // если произошло отпускание датчика
  } else if(digitalRead(CFG_EMENDSTOP_PIN) && EndPetTapeFlag) {
    EndPetTapeFlag = false;
    printTapeStatus();
  } 

  // Таймаут возврата интерфейса в IDLE 15 секунд
  if (currentMode != InerfaceMode::IDLE && (millis() - encLastActivity > 15000)) {
    currentMode = InerfaceMode::IDLE;
  }
}

void handleEncButton() {
  static bool lastSw = HIGH;
  static unsigned long pressStartTime = 0; // Время начала нажатия
  static bool longPressHandled = false;    // Чтобы не срабатывать по кругу при удержании

  uint8_t pins = PIND; 
  bool sw  = pins & ENC_MASK_SW;

  // 1. МОМЕНТ НАЖАТИЯ (Фронт вниз)
  if (sw == LOW && lastSw == HIGH) {
    pressStartTime = millis();
    longPressHandled = false;
    encLastActivity = millis();
  }

  // 2. ПРОВЕРКА УДЕРЖАНИЯ (Кнопка всё еще нажата)
  if (sw == LOW && !longPressHandled) {
    if (millis() - pressStartTime > 1000) { // Если держим больше 1 секунды
      // ДЕЙСТВИЕ НА УДЕРЖАНИЕ
      if ( currentMode != InerfaceMode::IDLE ) {
        if ( currentMode == InerfaceMode::EDIT_TEMP ) {
          Heat = ! Heat;
          printHeaterStatus();
        } else if ( currentMode == InerfaceMode::EDIT_SPEED ) {
          runMotor = ! runMotor;
          if(!runMotor) {
            currentSpeedX10 = SPEED_MIN10;
          }
          printMotorStatus();
        }
      } 
      longPressHandled = true; 
      encClickCount = 0; // Сбрасываем клики, чтобы не сработал обычный клик после отпускания
    }
  }

  // 3. МОМЕНТ ОТПУСКАНИЯ (Фронт вверх)
  if (sw == HIGH && lastSw == LOW) {
    if (!longPressHandled) { // Если это не было длинным нажатием
      unsigned long now = millis();
      if (now - encLastClickTime < 400) encClickCount++;
      else encClickCount = 1;
      encLastClickTime = now;
    }
  }

  lastSw = sw;

  // Проверить нажатие одно или два
  // и нарисовать на экране в инверсии нужную строку
  if (encClickCount > 0 && (millis() - encLastClickTime > 400)) {
    if (encClickCount == 1) { 
      currentMode = InerfaceMode::EDIT_TEMP;
      printTargetTemp();
    }
    else { 
      currentMode = InerfaceMode::EDIT_SPEED;
      printTargetSpeed();
    }
    encClickCount = 0;
  }
}

void emStop(int reason) {
  runMotor = false;
  Heat = false;
  heater_pwm_threshold = 0;
  digitalWrite(CFG_STEP_EN_PIN, HIGH); // Снять ток с мотора
  analogWrite(CFG_HEATER_PIN, 0);
  oled_clear();
  oled_printStrBig(0, 2, "*HALT!*", false);
  switch (reason) {
    case OVERHEAT:
      oled_printStrBig(0, 5, "Overheat", false);
      break;
    case THERMISTOR_ERROR:
      oled_printStrBig(0, 5, "Thermistor", false);
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

// и так все нужне переменные глобальные
int computePID(void) {

  if (!Heat) {
    pid_integral = 0; // Обнуляем "память" регулятора
    return 0;
  } 
   // Коэффициенты (подобраны с множителем 10)
  long Kp = (long)CFG_PID_P*10;
  long Ki = (long)CFG_PID_I*10;
  long Kd = (long)CFG_PID_D*10;

  // Лимиты для анти-виндапа (защита от разгона интеграла)
  const long i_limit = 2550; 
  long error = targetTemp10 - curTempX10;
  // 1. Пропорциональная часть
  long P = Kp * error;

  // 2. Интегральная часть (с защитой i_limit)
  pid_integral += error;
  if (pid_integral > i_limit) pid_integral = i_limit;
  else if (pid_integral < -i_limit) pid_integral = -i_limit;
  long I = Ki * pid_integral;

  // 3. Дифференциальная часть
  long D = Kd * (error - pid_lastError);
  pid_lastError = error;

  // Итоговый результат с обратным масштабированием
  // Делим на 100, так как K и Temp оба имеют множители
  long output = (P + I + D) / 100;

  // Ограничиваем под ШИМ 0-255
  if (output > 255) output = 255;
  if (output < 0) output = 0;

  return (int)output;
}