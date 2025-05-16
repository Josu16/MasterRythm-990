#include <Arduino.h>
#include <Audio.h>
// #include <AudioStream.h> 
/*
En este archivo se puede modificar el AUDIO_BLOCK_SAMPLES
Esto determina el tamaño del buffer de procesamiento de audio
entiendo que es similar al que se asigna en un controlador de audio en la pc
cuando se incrementa favorece el número de reproducciones simultáneas de samples de la SD,
sin embargo, Aun no se ha resuelto el problema de la polifonía, consultar:

https://forum.pjrc.com/index.php?threads/teensy-3-6-multiple-audioplaysdwav-players.55200/
*/ 
#include <Wire.h>
#include <SerialFlash.h>
#include "HyperNATURAL.h"

// #define NUM_VOICES 6

// GUItool: begin automatically generated code
AudioPlaySdWav           playWav1;     //xy=193,273
AudioPlaySdWav           playWav2;       //xy=196,325
AudioPlaySdWav           playWav3;       //xy=199,363
AudioPlaySdWav           playWav4;     //xy=205,410
AudioPlaySdWav           playWav5;     //xy=243,590
AudioPlaySdWav           playWav6;     //xy=245,631
AudioMixer4              mixer1;         //xy=463,430
AudioMixer4              mixer2;         //xy=465,502
AudioMixer4              Master_Mixer;         //xy=694,462
AudioOutputI2S           i2s1;           //xy=919,466
AudioConnection          patchCord1(playWav1, 0, mixer1, 0);
AudioConnection          patchCord2(playWav2, 0, mixer1, 1);
AudioConnection          patchCord3(playWav3, 0, mixer1, 2);
AudioConnection          patchCord4(playWav4, 0, mixer1, 3);
AudioConnection          patchCord5(playWav5, 0, mixer2, 0);
AudioConnection          patchCord6(playWav6, 0, mixer2, 1);
AudioConnection          patchCord7(mixer1, 0, Master_Mixer, 0);
AudioConnection          patchCord8(mixer1, 0, Master_Mixer, 1);
AudioConnection          patchCord9(mixer2, 0, Master_Mixer, 2);
AudioConnection          patchCord10(mixer2, 0, Master_Mixer, 3);
AudioConnection          patchCord11(Master_Mixer, 0, i2s1, 0);
AudioConnection          patchCord12(Master_Mixer, 0, i2s1, 1);
// GUItool: end automatically generated code

AudioControlSGTL5000     sgtl5000_1;

// Use these with the Teensy 3.5 & 3.6 & 4.1 SD card
#define SDCARD_CS_PIN    BUILTIN_SDCARD

// UBICACIÓN TEMPORAL
const char *notePaths[57] = {
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr,
    "/samples/snare/sonordesigner5.wav", //35
    "/samples/kick/sonordesigner5.wav", //36
    nullptr,
    nullptr, 
    nullptr, 
    nullptr, 
    "/samples/pandero/panderort5.wav", // 41
    "/samples/hat/zildjianquick5.wav", // 42
    nullptr, 
    nullptr,
    nullptr,
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    "/samples/ride/ridek5.wav", // 51
    nullptr, 
    nullptr,
    nullptr, 
    nullptr, 
    "/samples/cencerro/lprock5.wav"// 56
};

// UBICACIÓN TEMPORAL
AudioPlaySdWav *notePlayers[57] = {
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr,
    &playWav1, //35
    &playWav2, //36
    nullptr,
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr,// &playWav3, // 41
    nullptr,// &playWav3, // 42
    nullptr, 
    nullptr,
    nullptr,
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr, 
    nullptr,// &playWav5, // 51
    nullptr, 
    nullptr,
    nullptr, 
    nullptr, 
    nullptr// &playWav6// 56
};

HyperNATURAL::HyperNATURAL() {

}

void HyperNATURAL::initializeSG() {
  // Audio connections require memory to work.  For more
  // detailed information, see the MemoryAndCpuUsage example
  AudioMemory(60);
  /*
  10 bloques
  cada bloque tiene un tamaño fijo de 128 muestras.
  al trabajar con audio de 44.1kHz cada muestra ocupa 16 bits (2 bytes)
  por tanto cada bloque de memoria tiene un tamaño fijo de 128 x 2 bytes (muestrta) = 256 bytes por bloque
  Este buffer se almacena en la RAM2, y con prueba y error se puede asignar un buffer de 1500 muestras
  consumiendo el 76% de la memoria.
  Teóricamente con 1500 bloques y considerando que cada audio consume 3 bloques se tiene una polifonía máxima
  de 1500 / 3 = 500 Sonidos simultáneos.

  A este cálculo se le debe incrementar el consumo de bloques por efectos, conexiones, etc.
  Serial.println(AudioMemoryUsageMax());
  */

  // Comment these out if not using the audio adaptor board.
  // This may wait forever if the SDA & SCL pins lack
  // pullup resistors
  sgtl5000_1.enable();
  sgtl5000_1.volume(0.5);
  Serial.println("Iniciando depuración en USB Serial...");

  if (!(SD.begin(SDCARD_CS_PIN))) {
      // stop here, but print a message repetitively
      while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
      }
  }
}

void HyperNATURAL::playFile(uint8_t note)
{
  // Serial.print("Playing file: ");
  // Serial.println(filename);

  // Start playing the file.  This sketch continues to
  // run while the file plays.
  
  switch (note) {
    case 36:
      playWav1.play(notePaths[note]);
      break;
    case 35:
      playWav2.play(notePaths[note]);
      break;
    case 42:
      playWav3.play(notePaths[note]);
      break;
    case 56:
      playWav4.play(notePaths[note]);
      break;
    case 51:
      playWav5.play(notePaths[note]);
      break;
    case 41:
      playWav6.play(notePaths[note]);
      break;
  }
  // playWav.play(filename);

  // A brief delay for the library read WAV info
  // delay(25);

  // Simply wait for the file to finish playing.
  // while (playWav1.isPlaying()) {
    // uncomment these lines if you audio shield
    // has the optional volume pot soldered
    //float vol = analogRead(15);
    //vol = vol / 1024;
    // sgtl5000_1.volume(vol);
  // }
}

void HyperNATURAL::playSounds(HNBuffer &soundsPlaying) {
  uint8_t note;
  uint8_t velocity;
  while (!soundsPlaying.isEmpty()) {
    if ( soundsPlaying.dequeue(note, velocity)) {
      // Serial.print("Nota extraída: ");
      // Serial.println(note);
      // Serial.println(notePaths[note]);
      if (note >= 0 && note <= 56 && notePaths[note] != nullptr) {
        playFile(note);  // Reproduce el archivo correspondiente
        Serial.print("Reproduciendo: ");
        Serial.println(notePaths[note]);
      }
    }
  }
}