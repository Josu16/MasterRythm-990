#include "ABRSequencer.h"
#include <Arduino.h>
#include <SdFat.h>

ABRSequencer* ABRSequencer::instance = nullptr;

ABRSequencer::ABRSequencer(/*int pinARe, int pinbRe, int pinFw, long bpm, volatile uint32_t *triangleX*/ uint8_t PPQN)
    :
    controls(120, 1), // tempo por defecto
    screens(valuesMainScreen),
    parser(midiFiles, pattern)
{
    // Seteos generales del secuenciador:
    pulsesPerQuarterNote = PPQN;
    
    // Interfaz gráfica:
    valuesMainScreen.numberPtrn = 1;

    // Inicialización MIDI
    Serial7.begin(31250);     // Serial MIDI a 31,250 baudios

    // Indicadores
    pinMode(14, OUTPUT);
    pinMode(15, OUTPUT);

    // Vincula la instancia actual a la referencia estática
    instance = this;

}

void ABRSequencer::readAllPatterns() {
    parser.getAvailablePatterns();
}

void ABRSequencer::initializePattern(bool playing) {

    parser.parseFile(controls.readPtrn());
    // Copia segura del nuevo nombre
    strncpy(valuesMainScreen.namePtrn, pattern.tackName, 15);

    Serial.print("Contenido de namePtrn tras la copia: ");
    Serial.println(valuesMainScreen.namePtrn);

    // No es necesario agregar '\0' manualmente ya que strncpy respeta el límite asegurado por el memset

    patternLength = parser.getNumEvents();

    // for (unsigned int indexEvent = 0; indexEvent < patternLength; indexEvent++) {
    //     Serial.print("Type: ");
    //     Serial.print(pattern.events[indexEvent].type, HEX);
    //     Serial.print(", Note: ");
    //     Serial.print(pattern.events[indexEvent].note);
    //     Serial.print(", Velocity: ");
    //     Serial.print(pattern.events[indexEvent].velocity);
    //     Serial.print(", Tick: ");
    //     Serial.println(pattern.events[indexEvent].tick);
    // }

    // secuenciación 
    currentTick = 0;
    lastBpm = pattern.tempo;
    valuesMainScreen.bpm = lastBpm;
    isPlaying = playing;

    // Controles
    controls.setBpm(lastBpm);
    lastPtrn = controls.readPtrn();
    valuesMainScreen.numberPtrn = lastPtrn;

    // Actualizar timer y tempo
    // Ubicación temporal en función de la funcionalidad,
    // acá, al cambiar de ritmo se cambiará el tempo inmediatamente.
    updateTimerInterval();
}

void ABRSequencer::beginSequencer() {
    readAllPatterns();
    initializePattern();
}

void ABRSequencer::updateTimerInterval() {
    uint32_t interval = 60000000 / (valuesMainScreen.bpm * pulsesPerQuarterNote); // Intervalo en microsegundos
     // Detén el timer antes de actualizar el intervalo
    mainTimer.end();
    // Reinicia el timer con el nuevo intervalo
    mainTimer.begin(timerCallback, interval);
    // Muestra el intervalo calculado en la consola
    Serial.print("Intervalo actualizado: ");
    Serial.println(interval);
}

void ABRSequencer::timerCallback() {
    if (instance) {
        instance->onTimer();
    }
}

void ABRSequencer::onTimer() {
    // Esta función se ejecuta cada 5208 microsegundos (cuando el tempo está a 120)

    if (!isPlaying){

    }
    else {
        // Control del LED para cada nota negra (96 ticks) y semi-corchea (24 ticks)
        if (currentTick % 96 == 0) {
            // Encender el LED en cada nota negra
            digitalWrite(playLed, HIGH);
            playledState = true;
            playLedOffTick = currentTick + 24; // Apagar el LED después de 24 ticks
        }
        if (playledState && currentTick >= playLedOffTick) {
            // Apagar el LED después de 24 ticks
            digitalWrite(playLed, LOW);
            playledState = false;
        }

        for (unsigned int indexEvent = 0; indexEvent < patternLength; indexEvent++) {
            // Enviar el evento MIDI
            // TODO: revisar esta condición, mejorar el cíclo para iterar sobre la misma marcha del avance del tick
            // porque como está actualmente es ineficiente, recorre todos los eventos para buscar los que tocan ser 
            // ejecutados.
            if (pattern.events[indexEvent].tick == currentTick) {
                // Enviar el evento MIDI
                Serial7.write(pattern.events[indexEvent].type);
                Serial7.write(pattern.events[indexEvent].note);
                Serial7.write(pattern.events[indexEvent].velocity);
            }
        }
        updateTrianglePosition();

        currentTick++;  // Incrementa el contador de ticks

        // Reinicia el patrón después de 384 ticks (4/4 en 96 PPQN)
        if (currentTick >= pattern.totalTicks) {
            currentTick = 0;  // Reinicia los ticks
        }
    }
}

void ABRSequencer::updateBpm() {
    valuesMainScreen.bpm = controls.readBpm();
    // Solo actualiza el timer si el BPM ha cambiado
    if (valuesMainScreen.bpm != lastBpm) {
        updateTimerInterval(); // Ajusta el tempo llamando a la función de actualización del timer
        lastBpm = valuesMainScreen.bpm;         // Actualiza el valor de BPM previo
    }
}



void ABRSequencer::loop() {
    // Actualizaciones de pantallas.
    screens.refresh_ui();
    
    // Verificación de controles físicos.
    updateBpm();

    // Verificar cambios en el estado del footswitch
    if (controls.checkForFootswitch()) {
        isPlaying = !isPlaying;
        if (!isPlaying) {
            // Detener la secuencia
            currentTick = 0;  // Reinicia los ticks
            digitalWrite(playLed, LOW);
            allNotesOff(1);
            // Serial.println("Secuencia detenida.");
        }
    }

    // Verificar cambio de patrón (Radical MODE)
    valuesMainScreen.numberPtrn = controls.readPtrn();
    if (valuesMainScreen.numberPtrn != lastPtrn) {
        lastPtrn = valuesMainScreen.numberPtrn;
        // Detener todo evento midi mandado.
        allNotesOff(1);
        // Obterner el patrón
        Serial.print("Patrón: ");
        Serial.println(controls.readPtrn());
        initializePattern(isPlaying);
    }
}

uint32_t ABRSequencer::getCurrentTick() {
    return currentTick;
}

void ABRSequencer::updateTrianglePosition() {
  if (currentTick % 24 == 0) { // cambia la posición del triángulo cada Semicorchea.
    // Actualiza la posición del triángulo
    valuesMainScreen.triangleX = map(currentTick, 0, pattern.totalTicks, 10, 117);
  }
}

void ABRSequencer::allNotesOff(uint8_t channel) {
    // Asegúrate de que el canal esté en el rango MIDI (1-16)
    if (channel < 1 || channel > 16) return;

    uint8_t status = 0xB0 | (channel - 1); // Status byte: Control Change en el canal
    uint8_t controller = 123;             // Número de controlador: All Notes Off
    uint8_t value = 0;                    // 3er byte: Siempre 0x00

    // Enviar el mensaje MIDI
    Serial7.write(status);   // Enviar el Status byte
    Serial7.write(controller); // Enviar el número de controlador
    Serial7.write(value);    // Enviar el valor (0)
}