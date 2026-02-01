#include "functions.h"
#include "oled.h"
#include "globals.h"

void SplashScreen(void) {
  oled_printStrBig(22, 2, "APETctl", false);
  oled_printStr(31, 5, "asper V 2.0", false);

  uint8_t startX = 26;
  for (uint8_t i = 0; i < 3; i++) {
    oled_drawChinese(startX + (i * 26), 5, i); // 24 ширина + 2 интервал = 26
  }

  delay(2500);
 
  oled_clear();
  oled_printStr(74, 1, "*C", false);
  oled_printStr(62, 3, "mm/s", false);
  oled_printChar(98, 7, 'm', false);
}

void printMillageAndSpeed(long m, long s) {

  if(s > SPEED_MAX10) {
    s = SPEED_MAX10;
  }
  char buf[7]; // "99.999" + \0
  buf[6] = '\0'; // Завершаем строку
  buf[2] = '.';  // Точка всегда на этом месте
  
  // максимальное число милиметров 99999
  // в метрах 99.999
  buf[5] = (m % 10) + '0'; 
  long val = m / 10; 
  buf[4] = (val % 10) + '0';
  val /= 10; 
  buf[3] = (val % 10) + '0';
  val /= 10;
  buf[1] = (val % 10) + '0';
  buf[0] = (val / 10) + '0';
  oled_printStrBig(12, 6, buf, false);
  
  // скорость максимум 99
  buf[3] = '\0'; // Завершаем строку
  buf[1] = '.';  // Точка всегда на этом месте
  buf[2] = (s % 10) + '0';
  buf[0] = (s / 10) + '0';
  // вывести реальную (не расчетную скорость)
  oled_printStrBig(12, 2, buf, false);
}

void printTargetTemp(){
      if(currentMode == InerfaceMode::EDIT_TEMP)  {
        oled_printInt(88, 0, targetTemp10/10, true, true);
      } else {
        oled_printInt(88, 0, targetTemp10/10, true, false);
      }
}

//Входной параметр температура X10
void printCurrentTemp() {
  char buf[6]; // "123.5" + \0
  buf[5] = '\0'; // Завершаем строку
  buf[3] = '.';  // Точка всегда на этом месте
  
  // Дробная часть (последняя цифра)
  buf[4] = (curTempX10 % 10) + '0';
  int val = curTempX10 / 10; // Целая часть
  // Единицы
  buf[2] = (val % 10) + '0';
  val /= 10;
  // Десятки
  if (val > 0) buf[1] = (val % 10) + '0';
  else buf[1] = ' '; // Вместо нуля — пробел
  val /= 10;
  // Сотни
  if (val > 0) buf[0] = (val % 10) + '0';
  else buf[0] = ' '; // Вместо нуля — пробел
  oled_printStrBig(12, 0, buf, false);
}

void printTargetSpeed(){
  char buf[4]; // "9.9" + \0
  buf[3] = '\0';
  buf[1] = '.';
  buf[2] = (targetSpeedX10 % 10) + '0';
  buf[0] = (targetSpeedX10 / 10) + '0';
  if(currentMode == InerfaceMode::EDIT_SPEED)  {
    oled_printStrBig(88, 2, buf, true);
  } else {
    oled_printStrBig(88, 2, buf, false);
  }
}

void printHeaterStatus() {
  if(Heat) 
    oled_printCharBig(0, 0, '*', false);
  else
    oled_printCharBig(0, 0, '.', false);
}

void printMotorStatus() {
  if(runMotor) 
    oled_printCharBig(0, 2, '*', false);
  else
    oled_printCharBig(0, 2, '.', false);
}


void printTapeStatus() {
  if(EndPetTapeFlag) 
    oled_printCharBig(0, 6, 'X', false);
  else
    oled_printCharBig(0, 6, ' ', false);
}


// Вызывать управление мотором каждые 100мс из основного цикла
void motorCTL() {
  static uint8_t currentSpeedX10 = SPEED_MIN10; 
  static bool motorPreviosRunState = false;
 
  uint16_t period;

  if (runMotor) { 
    // Включить если выключен
    if(!motorPreviosRunState) {
      digitalWrite(CFG_STEP_EN_PIN, LOW);
      delay(1); 
      motorPreviosRunState =  true;
    }

    if (currentSpeedX10 != targetSpeedX10) {
      if (currentSpeedX10 < targetSpeedX10) { currentSpeedX10++; } 
      else if (currentSpeedX10 > targetSpeedX10) { currentSpeedX10--; }
      period = pgm_read_word(&step_table[currentSpeedX10]);
      noInterrupts();
      OCR1A = period;
      // ЕСЛИ новый период меньше текущего счетчика, сбрасываем счетчик,
      // чтобы мотор не ждал полного круга таймера в 65535 тиков
      if (TCNT1 >= period) TCNT1 = 0; 
      interrupts();
    }
  } else {
    // Мотор выключили:
    if(motorPreviosRunState) {
      digitalWrite(CFG_STEP_EN_PIN, HIGH); // Выключаем удержание  
      motorPreviosRunState = false;
      // также установить максимальный интервал между шагами
      // чтобы потом разгоняться с нуля.
      currentSpeedX10 = SPEED_MIN10;
      noInterrupts();
      OCR1A = pgm_read_word(&step_table[currentSpeedX10]);
      interrupts();
    }
  }
}

void emStop(int reason) {
  runMotor = false;
  Heat = false;
  heater_pwm_threshold = 0;
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
  digitalWrite(CFG_STEP_EN_PIN, HIGH); // Снять ток с мотора
  analogWrite(CFG_HEATER_PIN, 0);      // Отлключить нагреватель
  for(;;){
    delay(60000);
  }
}

// и так все нужне переменные глобальные
//__attribute__((noinline)) uint32_t computePID(void) {
uint32_t computePID(void) {
  static long pid_integral = 0;
  static long pid_lastError = 0;
  // Интеграл должен уметь "заполнить" весь ШИМ
  const long i_limit = (long)CFG_PID_I_LIMIT;
  
  if (!Heat) {
    pid_integral = 0; // Обнуляем "память" регулятора
    return 0;
  } 
  // Коэффициенты (подобраны с множителем учетем что макс ШИМ 33333)
  long Kp = (long)CFG_PID_P;
  long Ki = (long)CFG_PID_I;
  long Kd = (long)CFG_PID_D;

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
  long long total = (long long)P + I + D;
  long output = (long)(total >> 10);

  // ШИМ 30 герц, таймер 2МГц итого 33333 микросекунт на максимальное значение шим
  // Ограничиваем под ШИМ 
  if (output > 33333) output = 33333;
  if (output < 0) output = 0;

  return (uint32_t)output;
}
