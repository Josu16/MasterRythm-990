#ifndef  SGSETTINGS_H
#define SGSETTINGS_H

/*
Esta clase está estrechamente relacionada con HyperNaturalSoundGenerator.h/.cpp
localizada en SoundGenerator/HyperNatural/src/ porque sus parámetros y configuración
definen los parámetros de esta clase, por ejemplo: MAX_INSTRUMENTS
*/

#include <Arduino.h>

static constexpr int MAX_INSTRUMENTS = 20;
// Tamaño máximo de caracteres permitidos para un instrumento
static constexpr int MAX_SIZE_INSTRUMENT_NAME = 50;

struct Instrument {
    int nota;
    char nombre[50];
    float instrumentGain = 0.0f;
};

class SGSettings {
   private:
      Instrument instruments[MAX_INSTRUMENTS];
      int totalInstruments = 0;
   public:
      SGSettings();
      bool readInfoInstruments();
};

#endif