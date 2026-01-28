#include "functions.h"
#include "PETCTL_cfg.h"

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

void printMillageAndSpeed(float m, float s) {
  
  if(s > (float)SPEED_MAX) {
    s = (float)SPEED_MAX;
  }
 
  // Вывести Метраж, мусора не будет т.к. только нарастает :)
  oled.setScale(2);
  oled.setCursorXY(12, 47);
  oled.print(m, 3);  
  // вывести реальную (не расчетную скорость)
  oled.setCursorXY(12, 23);
  oled.print(s, 1);
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

void printTargetSpeed(long s){
      // s -speed in mm/s * 10
      // // pint in mm/s
      oled.setScale(2);      
      oled.setCursorXY(88, 23);
      if(whatToChange == CHANGE_SPEED)  oled.invertText(true);
      oled.print((float)s/10, 1);
//      if (s<100) oled.print(" "); //fix display garbage 
      oled.invertText(false);
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

