#include <Arduino.h>
#include <Wire.h>
#include "oled.h"
#include "charMap.h"

// 1. Отправка команды на экран
void oled_cmd(uint8_t cmd) {
    Wire.beginTransmission(OLED_ADDR);
    Wire.write(0x00); 
    Wire.write(cmd);
    Wire.endTransmission();
}

// 2. Инициализация (минимальный набор)
void oled_init() {
    Wire.begin();
    Wire.setWireTimeout(3000, true);
    Wire.setClock(400000);
    uint8_t init[] = {
      0xAE,             // Дисплей выкл
      0xD5, 0x80,       // Частота осциллятора
      0xA8, 0x3F,       // Высота (64 строки)
      0xD3, 0x00,       // Смещение (нет)
      0x40,             // Начальная строка 0
      0x8D, 0x14,       // ВКЛЮЧИТЬ ПИТАНИЕ (Charge Pump)
      0x20, 0x02,       // Режим адресации: Page Addressing Mode (как у Гайвера)
      0xA1,             // Отражение по X
      0xC8,             // Отражение по Y
      0xDA, 0x12,       // Конфигурация COM-пинов
      0x81, 0xCF,       // Яркость (от 00 до FF)
      0xD9, 0xF1,       // Период предзаряда
      0xDB, 0x40,       // Уровень VCOMH
      0xA4,             // Выводить содержимое памяти (не инверсию)
      0xA6,             // Обычный режим (не негатив)
      0xAF              // ДИСПЛЕЙ ВКЛ
    };
    for (uint8_t i = 0; i < sizeof(init); i++) oled_cmd(init[i]);
}

// 3. Очистка экрана (заливка нулями)
void oled_clear() {
    for (uint8_t p = 0; p < 8; p++) {
        oled_cmd(0xB0 + p); // Установка страницы (строки)
        oled_cmd(0x00);     // X младшие 4 бита = 0
        oled_cmd(0x10);     // X старшие 4 бита = 0
        
        // 128 пикселей делим на 8 заходов по 16 байт
        // (16 байт — самый безопасный размер для Wire)
        for (uint8_t chunk = 0; chunk < 8; chunk++) {
            Wire.beginTransmission(OLED_ADDR);
            Wire.write(0x40); // Режим данных
            for (uint8_t i = 0; i < 16; i++) {
                Wire.write(0x00);
            }
            Wire.endTransmission();
        }
    }
}

// 4. Печать одного символа (по упрощенной сетке X, Page)
// x: 0-127, page: 0-7 (высота 8 пикселей)
void oled_printChar(uint8_t x, uint8_t page, char c, bool inv = false) {
    oled_cmd(0xB0 + page);
    oled_cmd(0x00 | (x & 0x0F));
    oled_cmd(0x10 | (x >> 4));

    // Если символ вне диапазона таблицы (например, < 32 или > 127)
    // Заменяем его на пробел (0x20)
    if (c < 0x20 || c > 0x7F) c = 0x20; 

    Wire.beginTransmission(OLED_ADDR);
    Wire.write(0x40);
    for (uint8_t i = 0; i < 5; i++) {
        uint8_t col = pgm_read_byte(&charMap[c - 0x20][i]);
        Wire.write(inv ? ~col : col);
    }
    Wire.write(inv ? 0xFF : 0x00); // Межсимвольный интервал
    Wire.endTransmission();
}

// 5. Печать строки
void oled_printStr(uint8_t x, uint8_t page, const char* str, bool inv = false) {
    while (*str) {
        oled_printChar(x, page, *str++, inv);
        x += 6; // Сдвиг на ширину символа + интервал
        if (x > 122) break; // Защита от вылета за край
    }
}


// Вспомогательная функция для растягивания 4 бит в 8 (каждый бит дублируется)
// 0000ABCD -> AABBCCDD
uint8_t stretch(uint8_t x) {
    uint8_t res = 0;
    for (uint8_t i = 0; i < 4; i++) {
        if (x & (1 << i)) res |= (0b11 << (i * 2));
    }
    return res;
}

// Печать большой буквы (10x16 пикселей + 2 интервал)
void oled_printCharBig(uint8_t x, uint8_t page, char c, bool inv = false) {
    // Символ в массиве имеет 5 байт (колонок). Для большой буквы нужно 10 колонок.
    for (uint8_t part = 0; part < 2; part++) {
        oled_cmd(0xB0 + page + part);
        oled_cmd(0x00 | (x & 0x0F));
        oled_cmd(0x10 | (x >> 4));

        // Если символ вне диапазона таблицы (например, < 32 или > 127)
        // Заменяем его на пробел (0x20)
        if (c < 0x20 || c > 0x7F) c = 0x20; 

        Wire.beginTransmission(OLED_ADDR);
        Wire.write(0x40);
        for (uint8_t i = 0; i < 5; i++) {
            uint8_t col = pgm_read_byte(&charMap[c - 0x20][i]);
            // Если part 0, берем младшие 4 бита, если 1 — старшие
            uint8_t half = (part == 0) ? (col & 0x0F) : (col >> 4);
            uint8_t stretched = stretch(half);
            
            uint8_t out = inv ? ~stretched : stretched; // Инверсия
            Wire.write(out); // Дублируем колонку для ширины
            Wire.write(out); 
        }
        Wire.write(inv ? 0xFF : 0x00); // Межсимвольный интервал (2 пикселя)
        Wire.write(inv ? 0xFF : 0x00); // Инверсный интервал
        Wire.endTransmission();
    }
}

// Печать большой строки
void oled_printStrBig(uint8_t x, uint8_t page, const char* str, bool inv = false) {
    while (*str) {
        oled_printCharBig(x, page, *str++, inv);
        x += 12; // 10 пикселей буква + 2 интервал
        if (x > 115) break;
    }
}

void oled_printInt(uint8_t x, uint8_t page, long num, bool big = false, bool inv = false) {
    char buf[12]; // Буфер для числа (хватит даже для -2147483648)
    itoa(num, buf, 10); // Преобразуем число num в строку buf в 10-ричной системе
    if (big) oled_printStrBig(x, page, buf, inv);
    else oled_printStr(x, page, buf, inv);
}

/*
// num - число, width - общая ширина строки, prec - знаков после запятой
void oled_printFloat(uint8_t x, uint8_t page, float num, uint8_t prec, bool big = false, bool inv = false) {
    char buf[10]; 
    // dtostrf(число, мин_ширина, знаков_после_запятой, буфер)
    dtostrf(num, 1, prec, buf); 
    if (big) oled_printStrBig(x, page, buf, inv);
    else oled_printStr(x, page, buf, inv);
}
*/

void oled_drawChinese(uint8_t x, uint8_t page, uint8_t index) {
    for (uint8_t p = 0; p < 3; p++) { // Рисуем 3 страницы в высоту
        oled_cmd(0xB0 + page + p);
        oled_cmd(0x00 | (x & 0x0F));
        oled_cmd(0x10 | (x >> 4));

        // Разбиваем отправку на чанки по 12 байт (безопасно для Wire)
        for (uint8_t chunk = 0; chunk < 2; chunk++) {
            Wire.beginTransmission(OLED_ADDR);
            Wire.write(0x40);
            for (uint8_t i = 0; i < 12; i++) {
                // Смещение: (номер страницы * 24 пикселя ширины) + колонка
                uint8_t b = pgm_read_byte(&heart_font[index][p * 24 + chunk * 12 + i]);
                Wire.write(b);
            }
            Wire.endTransmission();
        }
    }
}
