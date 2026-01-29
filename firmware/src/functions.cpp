#include "functions.h"
#include "PETCTL_cfg.h"
#include "oled.h"

void SplashScreen(void) {
  oled_printStrBig(22, 2, "APETctl", false);
  oled_printStr(31, 5, "asper V 1.0", false);

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
  
  if(s > (float)SPEED_MAX) {
    s = (float)SPEED_MAX;
  }
 
  // Вывести Метраж, мусора не будет т.к. только нарастает :)
  oled_printFloat(12, 6, m, 3, true, false);
  // вывести реальную (не расчетную скорость)
  oled_printFloat(12, 2, s, 1, true, false);
}

void printTargetTemp(long t){
      if(whatToChange == CHANGE_TEMPERATURE)  {
        oled_printInt(88, 0, t, true, true);
      } else {
        oled_printInt(88, 0, t, true, false);
      }
}

//Входной параметр температура X10
void printCurrentTemp(long t) {
  char buf[6]; // "123.5" + \0
  buf[5] = '\0'; // Завершаем строку
  buf[3] = '.';  // Точка всегда на этом месте
  
  // Дробная часть (последняя цифра)
  buf[4] = (t % 10) + '0';
  int val = t / 10; // Целая часть
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

void printTargetSpeed(long s){
  if(whatToChange == CHANGE_SPEED)  {
    oled_printFloat(88, 2, (float)s/10.0, 1, true, true);
  } else {
    oled_printFloat(88, 2, (float)s/10.0, 1, true, false);
  }
}

void printHeaterStatus(bool status) {
  if(status) 
    oled_printCharBig(0, 0, '*', false);
  else
    oled_printCharBig(0, 0, '.', false);
}

void printMotorStatus(bool status) {
  if(status) 
    oled_printCharBig(0, 2, '*', false);
  else
    oled_printCharBig(0, 2, '.', false);
}


void printTapeStatus(bool status) {
  if(status) 
    oled_printCharBig(0, 6, 'X', false);
  else
    oled_printCharBig(0, 6, ' ', false);
}
