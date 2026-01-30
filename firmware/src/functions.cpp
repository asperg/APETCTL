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

void printMillageAndSpeed(float m, float s) {

  if(s > (float)SPEED_MAX10/10.0) {
    s = (float)SPEED_MAX10/10.0;
  }
 
  // Вывести Метраж, мусора не будет т.к. только нарастает :)
  oled_printFloat(12, 6, m, 3, true, false);
  // вывести реальную (не расчетную скорость)
  oled_printFloat(12, 2, s, 1, true, false);
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
  if(currentMode == InerfaceMode::EDIT_SPEED)  {
    oled_printFloat(88, 2, (float)targetSpeedX10/10.0, 1, true, true);
  } else {
    oled_printFloat(88, 2, (float)targetSpeedX10/10.0, 1, true, false);
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
