#ifndef FUNCTIONS_H
#define FUNCTIONS_H
#include <Arduino.h>

// from main.cpp
void emStop(int reason);
void motorCTL(long setSpeedX10);
void encRotationToValue (long* value, int inc, long minValue, long maxValue);
void interactiveSet();
long getTemp();
int computePID(void);
void LengthEventISR(void);
void interfaceEncoderISR();
void handleEncButton();

//from functions.cpp
void printTargetTemp();
void printCurrentTemp();
void printTargetSpeed();
void printHeaterStatus();
void printMotorStatus();
void printTapeStatus();
void SplashScreen(void);
void printMillageAndSpeed(float m, float s);

#endif