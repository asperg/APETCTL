#include <Arduino.h>
#include <avr/wdt.h>
#include <PinChangeInterrupt.h>
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
  SET_LED_OUTPUT;

  // Настройка Таймера, для работы двигателя
  noInterrupts();
  TCCR1A = 0;
  TCCR1B = _BV(WGM12) | _BV(CS11); // Режим CTC, предделитель 8 (0.5 мкс)
  OCR1A = pgm_read_word(&step_table[CFG_SPEED_INIT]); // Начальное значение
  TIMSK1 |= _BV(OCIE1A);           // Разрешить прерывание
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

  // Разрешаем аппаратные прерывания INT0 и INT1
  EICRA |= (1 << ISC00) | (1 << ISC10); // Устанавливаем режим CHANGE для обоих
  EIMSK |= (1 << INT0) | (1 << INT1);   // Включаем их

  printTargetSpeed();
  printTargetTemp();
  printHeaterStatus();
  printMotorStatus();
  printTapeStatus();
  // только закоментив эту функцию сэкономил 1% памяти
  // т.к. она заинлайнилась после этого
  //printMillageAndSpeed(0, 0);
}


ISR(INT0_vect) {
  interfaceEncoderISR();
}

ISR(INT1_vect) {
  interfaceEncoderISR();
}

// обработчики прерываний
// обработчик двигателя, шагаем двигателм здесь !!!!!
ISR(TIMER1_COMPA_vect) {
  static uint32_t heater_timer_acc = 0;
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
    if (heater_timer_acc > heater_pwm_threshold) {
      PORTB &= ~heater_bit;
    }
  }
}

// обработчик энкодера интерфейса
void interfaceEncoderISR() {
  static uint8_t state = 0xFF;
  uint8_t currentState = (PIND >> 2) & 0x03;
  //currentState ^= 0x03; // Сдвиг фазы энкодера если пропукает шаг при смене направления
  state = (state << 2) | currentState;
  if ( state == 0b11010010 ) { encDelta--; }
  else if ( state == 0b11100001 ) { encDelta++; }
}

// Обработка прерывания от датчика длины прутка
void LengthEventISR(void) {
  // debug code
  //digitalWrite(13, !digitalRead(13));
  //TOGGLE_LED;

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
    // Здесь же управление мотором
    motorCTL();
    spinnerTimer = millis();
    oled_printChar(120, 7, spinner[spinnerIdx], false);
    // В двое медленне, здесь опрос температуры !!! и расчет пида
    if (spinnerIdx % 2) {
      // TEMPERATURE ---------------------------
      curTempX10 = getTemp();
      if (curTempX10 > CFG_TEMP_MAX_X10 - 100) emStop(OVERHEAT);
      if (Heat) {
        uint32_t new_heater_pwm_thresold = computePID();
        // ШИМ 30 герц, таймер 2МГц итого 33333 микросекунт на максимальное значение шим
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
    // если нагрутили энкодером
    if (encDelta != 0) {
      noInterrupts();
      int8_t copyDelta = encDelta;
      encDelta = 0;
      interrupts();
      if (currentMode != InerfaceMode::IDLE) {
        encLastActivity = millis();
        int8_t dir = (copyDelta > 0) ? 1 : -1;
        int8_t absDelta = abs(copyDelta);
        if (absDelta > 1)  { copyDelta = (absDelta - 1) * 5 * dir; } // Если быстро крутил ручку
        if (currentMode == InerfaceMode::EDIT_TEMP) {
          targetTemp10 = constrain(targetTemp10 + copyDelta*10, CFG_TEMP_MIN_X10, CFG_TEMP_MAX_X10);
          printTargetTemp();
        } else if (currentMode == InerfaceMode::EDIT_SPEED) {
          targetSpeedX10 = constrain(targetSpeedX10 + copyDelta, SPEED_MIN10, SPEED_MAX10);
          printTargetSpeed();
        }
      }
    }
    if (++spinnerIdx >= 4) spinnerIdx = 0;
  }

  // Если новых импульсов нет больше 5 секунд — считаем, что скорость 0
  // флаг указывает на то что ранее измеренная скорость не равна 0
  // и плавно заполняем кольцевой буфер ULONG_MAX / 32
  noInterrupts();
  unsigned long copyLastTimeInterrupt = lastTimeInterrupt;
  interrupts();
  if (SlimStopFlag && micros() - copyLastTimeInterrupt > 5000000) {
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
    long CurrentFilamentSpeed;
    if(avgDuration == (~0UL) >> 5) {
      CurrentFilamentSpeed = 0;
      // Отключить проверку на наличие прерываний
      // для плавного уменьшения показателя скорости на экране
      // т.к. скорость уже 0
      SlimStopFlag = false;
    } else if(avgDuration != 0) {
      CurrentFilamentSpeed = 10000000L / avgDuration;
      SlimStopFlag = true;
    } else {
      CurrentFilamentSpeed = 0;
      SlimStopFlag = false;
    }
    // Ролик диаметр 10мм длина окружности 31.4159 мм
    // на кольце энкодера 32 отверсия, т.е. 1-тик почти 1мм
    // на полметра набегает погрешность в 9 мм (насчитывает больше чем надо)
    // вот и ввел коэффициент пересчета каждые полметра онимаю 9мм
    long FilamentLength = copyFilamentTiks - (copyFilamentTiks>>9)*9;
    printMillageAndSpeed(FilamentLength, CurrentFilamentSpeed);
  }

  handleEncButton();

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
    printTargetTemp();
    printTargetSpeed();
  }
}

void handleEncButton() {
  static bool lastSw = HIGH;
  static unsigned long pressStartTime = 0; // Время начала нажатия
  static bool longPressHandled = false;    // Чтобы не срабатывать по кругу при удержании
  static uint8_t encClickCount = 0;

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
      if ( currentMode == InerfaceMode::EDIT_TEMP ) {
        Heat = ! Heat;
        printHeaterStatus();
      } else if ( currentMode == InerfaceMode::EDIT_SPEED ) {
        runMotor = ! runMotor;
        printMotorStatus();
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
    if (encClickCount == 1) { currentMode = InerfaceMode::EDIT_TEMP; }
    else { currentMode = InerfaceMode::EDIT_SPEED; }
    printTargetTemp();
    printTargetSpeed();
    encClickCount = 0;
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
