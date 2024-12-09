#ifndef ABRSEQUENCER_H
#define ABRSEQUENCER_H

#include <arduino.h>
#include <IntervalTimer.h>
#include <vector>

#include "RotaryEncoder.h"
#include "MidiParser.h"
#include "UI.h"
#include "Control.h"

class ABRSequencer {
    private:
        uint8_t pulsesPerQuarterNote;
        IntervalTimer mainTimer;
        char name[20];
        uint8_t numerator;
        uint8_t denominator;
        volatile uint32_t currentTick = 0;
        uint16_t lastBpm;
        uint16_t lastPtrn;
        uint8_t lastVariant;
        uint8_t measures;
        uint32_t totalTicks;

        Pattern pattern;

        // Configuraión de variables para la reproducción (Máquina de Estados Finitos)
        unsigned int patternLength;
        enum SequencerState {
            STOPPED,    // Parado
            PLAYING    // Reproduciendo
        };
        volatile SequencerState currentState = STOPPED; // Estado inicial

        // Controles
        Control controls;

        // Indicadores
        const int playLed = 14;
        volatile bool playledState = false;     // Estado actual del LED
        volatile uint32_t playLedOffTick = 0;   // Tick en el que se apagará el LED

        // Función estática para el timer
        static void timerCallback();
        // Referencia estática a la instancia actual
        static ABRSequencer* instance;

        // Interfaz gráfica
        MainScreen valuesMainScreen;
        UI screens; // al declararlo se inicializa automáticamente por el compilador.

        // Parser
        MidiParser parser;
        std::vector<String> midiFiles; // Contenedor para los nombres de archivos


    public:
        ABRSequencer(/*int pinARe, int pinbRe, int pinFw, long bpm, volatile uint32_t *triangleX*/uint8_t PPQN);
        void beginSequencer();
        void readAllPatterns();
        void onTimer();
        void updateTimerInterval();
        void updateBpm();
        void loop(); // Método para manejar actualizaciones en el loop principal
        void initializePattern();
        void updateTrianglePosition();
        void allNotesOff(uint8_t channel);
        void transitionToState(SequencerState newState);
};

#endif