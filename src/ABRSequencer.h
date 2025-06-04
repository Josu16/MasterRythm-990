#ifndef ABRSEQUENCER_H
#define ABRSEQUENCER_H

#include <Arduino.h>
#include <IntervalTimer.h>
#include <vector>

#include "pins.h"
#include "RotaryEncoder.h"
#include "MidiParser.h"
#include "UI.h"
#include "Control.h"
#include "HyperNATURAL.h"
#include "HNBuffer.h"
#include "SGSettings.h"

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
        volatile unsigned int patternLength;  // número de eventos midi por patrón
        enum SequencerState {
            STOPPED,    // Parado
            PLAYING,   // Reproduciendo
            RESETING
        };
        volatile SequencerState currentState = STOPPED; // Estado inicial
        volatile bool autoChangePtrn = false;

        // Controles
        Control controls;
        uint8_t ptrnChangedAgain = 0;

        // Indicadores
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
        // std::vector<String> midiFiles; // Contenedor para los nombres de archivos
        std::vector<MidiFile> midiFiles;


        // HyperNATURAL Sound Generator
        HyperNATURAL soundGenerator;
        HNBuffer soundsPlaying;
        volatile bool fullBuffer;

        // VERSIÓN FINAL SUPER NATURAL SOUND GENERATOR (ANTERIOR INVÁLIDO)
        SGSettings soundGeneratorSettings;

    public:
        ABRSequencer(uint8_t PPQN);
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