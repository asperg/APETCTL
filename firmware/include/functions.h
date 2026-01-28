#ifndef FUNCTIONS_H
#define FUNCTIONS_H
#include <GyverOLED.h> 
extern GyverOLED<SSD1306_128x64, OLED_NO_BUFFER> oled;
extern int whatToChange;

void printTargetTemp(long t);
void printCurrentTemp(long t);
void printTargetSpeed(long s);
void printHeaterStatus(boolean status);
void printMotorStatus(boolean status);
void printTapeStatus(boolean status);
void SplashScreen(void);
void printMillageAndSpeed(float m, float s);


#endif