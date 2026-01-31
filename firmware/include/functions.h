#ifndef FUNCTIONS_H
#define FUNCTIONS_H
#include <Arduino.h>

// from main.cpp
void encRotationToValue (long* value, int inc, long minValue, long maxValue);
void interactiveSet();
long getTemp();
uint32_t computePID();
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
void motorCTL();
void emStop(int reason);

#endif