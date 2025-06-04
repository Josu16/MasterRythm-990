#ifndef UI_H
#define UI_H

#include <Arduino.h>
#include "pins.h"
#include <U8g2lib.h>
#include <SPI.h>

struct MainScreen{  // TODO: revisar el nombre final respecto a los nombres de los objetos de pantalla
    uint8_t numberPtrn;
    char namePtrn[16]; // Se agregó un espacio más de caracteres para que se utilice como el '\0'
    uint8_t measures;
    uint8_t numerator;
    uint8_t denominator;
    volatile uint32_t triangleX = 10;
    volatile uint8_t bpm;
    volatile uint8_t currentBlack = 0;
    volatile uint8_t currentMeasure = 1;
    volatile uint8_t currentVariationIndex = 1;
    volatile bool waitingForChangePtrn = false;
    bool loockTempo = false;
};

class UI {
    private:
        MainScreen &valuesMainScreen;

       // Configura las pantallas usando Hardware SPI en lugar de Software SPI
        U8G2_ST7565_ERC12864_1_4W_HW_SPI playScreen;
        U8G2_ST7565_ERC12864_1_4W_HW_SPI ptrScreen;

        // long patronIndex = 1;
        int ultimopatronIndex = -1;

        // volatile uint32_t triangleX = 10; // Posición actual del triángulo
        const int lineY = 41; // Y de la línea horizontal
        
       // methods
       void draw_dotted_line(U8G2 &u8g2, int x1, int y1, int x2, int y2);

    public:
        UI(MainScreen &values);  // revisar como actualizar cuando entre a otra perspectiva de pantalla (ej. configuración)
        void changeViewScreen();

        void refreshUi();
        void refreshPlayScreen();
        void refreshPtrnScreen();

        void paintVariation();

        void drawMixer();
};

#endif