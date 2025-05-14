//
// myclass.h
//
#ifndef HYPERNATURALSOUNDGENERATOR
#define HYPERNATURALSOUNDGENERATOR

#include <circle/logger.h>
#include <circle/types.h>
#include <circle/sound/soundbasedevice.h>
#include <circle/devicenameservice.h>
// #include <circle/fs/fat/fatfs.h>
#include <fatfs/ff.h>
#include <circle/util.h>
#include <circle/sched/scheduler.h>
#include <circle/serial.h>
#include <circle/timer.h>

// Máximo de voces simultáneas
static constexpr int MAX_VOICES = 32;

// Máximo de notas encoladas
static constexpr int MAX_PENDING_NOTES = MAX_VOICES * 2;

// Notas musicales
static constexpr int NUM_NOTES = 128;

// Máximo número de instrumentos (multiplicado por 6)
static constexpr int MAX_INSTRUMENTS = 20;

// Máximo número de capas por instrumento
static constexpr int MAX_SAMPLE_LAYERS = 6;

// Máximo número de wavs aceptados en la memoria RAM
static constexpr int MAX_WAV_FILES = 20;

struct WAVHeader {
	char chunkID[4];        // "RIFF"
	unsigned chunkSize;     // Tamaño total del archivo - 8 bytes
	char format[4];         // "WAVE"
	char subChunk1ID[4];    // "fmt "
	unsigned subChunk1Size; // Tamaño del subchunk "fmt" (16 para PCM)
	unsigned short audioFormat;  // Formato de audio (1 = PCM)
	unsigned short numChannels;  // Número de canales (1 = Mono, 2 = Estéreo)
	unsigned sampleRate;    // Frecuencia de muestreo (ej., 44100 Hz)
	unsigned byteRate;      // Bytes por segundo = SampleRate * NumChannels * BitsPerSample/8
	unsigned short blockAlign;   // BlockAlign = NumChannels * BitsPerSample/8
	unsigned short bitsPerSample; // Bits por muestra (16 bits = 2 bytes)
	char subChunk2ID[4];    // "data"
	unsigned subChunk2Size; // Tamaño de los datos de audio (en bytes)
};

struct WavDirectory {
		int nota;
		char nombre[12];
};	

struct SampleOffsets {
	size_t sampleSize;
	size_t startIndex;
};

struct Instrument {
   int nota;
	char nombre[20];
   char nombreSample[MAX_SAMPLE_LAYERS][15] = {'\0'};
   SampleOffsets samples[MAX_SAMPLE_LAYERS]; // INDICA EL NÚMERO MÁXIMO DE CAPAS DEL SAMPLE <----------------
   int numberLayers = 0;
};

// Estructura por voz activa
struct Voice {
    const SampleOffsets* sample;  // puntero a sampleInfo[]
    volatile size_t pos;    // bytes ya leídos
    float gain;   // volumen [0..1]
    volatile bool active = false; // voz en uso
};

// Estructura de cola circular para sincronizar los chunks de la interrupción y reducir jitter
struct PendingNote {
    u8 note;    // Número de la nota MIDI o similar
    u8 velocity; // dinámica de la nota.
    unsigned arrivalTime; // Tiempo de llegada en ticks o milisegundos
    bool used;  // Indica si esta entrada está ocupada
};

class HyperNaturalSoundGenerator
{
public:
   HyperNaturalSoundGenerator (CSoundBaseDevice &sound, CLogger &logger, CScheduler	&scheduler, CDeviceNameService &m_DeviceNameService, CSerialDevice &m_Serial, CTimer &m_Timer);

   int samplesCheck();

   bool loadSamplesOnRAM();
   
   void loop();

	~HyperNaturalSoundGenerator (void);

   // bool loadSamplesOnRam();

	// methods ...

private:
	// members ...
   CSoundBaseDevice &m_pSound;
   CLogger &m_Logger;
   CScheduler		m_Scheduler;
   CDeviceNameService &m_DeviceNameService;
   FATFS 			m_FileSystem;
   CSerialDevice m_Serial;
   CTimer &m_Timer;
   
   unsigned nQueueSizeFrames;

   SampleOffsets sampleInfo[100];
   WavDirectory wavMemory[MAX_WAV_FILES];
   Instrument instruments[MAX_INSTRUMENTS];
   int totalInstruments = 0;

   u8 *wavRoom;

   long totalSizeWavRoom;
   long usedSizeWavRoom;
   int totalSamples;
   // int numberWavs;

   int m_NoteToSample[NUM_NOTES];  // -1 = sin sample asignado

   Voice m_Voices[MAX_VOICES];


   PendingNote pendingNotes[MAX_PENDING_NOTES];
   volatile int pendingHead = 0;  // Índice para insertar nuevas notas
   volatile int pendingTail = 0;  // Índice para sacar notas

   void TriggerVoice(u8 note);
   void OnNeedData();
   static void OnNeedDataAdapter(void* ctx);
   void assignNoteToSample();
   void ProcessDirectory(const char *path, const char *parentPath);
   int extractNumber(const char *str);

};

#endif
