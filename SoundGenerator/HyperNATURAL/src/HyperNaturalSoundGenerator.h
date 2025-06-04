//
// myclass.h
//
#ifndef HYPERNATURALSOUNDGENERATOR
#define HYPERNATURALSOUNDGENERATOR

#include <circle/logger.h>
#include <circle/types.h>
#include <circle/sound/soundbasedevice.h>
#include <circle/devicenameservice.h>
#include <circle/multicore.h>
#include <circle/memory.h>
#include <fatfs/ff.h>
#include <circle/util.h>
#include <circle/sched/scheduler.h>
#include <circle/serial.h>
#include <circle/timer.h>
#include <circle/cputhrottle.h>
#include <circle/spinlock.h>
#include <circle/actled.h>

// Máximo de voces simultáneas
static constexpr int MAX_VOICES = 32;

// Máximo de notas encoladas (pendientes para ser reproducidas)
static constexpr int MAX_PENDING_NOTES = MAX_VOICES * 2;

// Notas musicales del estándar MIDI
static constexpr int NUM_NOTES = 128;

// Máximo número de instrumentos (multiplicado por 6)
static constexpr int MAX_INSTRUMENTS = 20;

// Máximo número de capas por instrumento (expresividad del generador de sonidos)
static constexpr int MAX_SAMPLE_LAYERS = 6;

// Máximo número de wavs aceptados en la memoria RAM
static constexpr int MAX_WAV_FILES = 20;

// Tamaño máximo de caracteres permitidos para un instrumento
static constexpr int MAX_SIZE_INSTRUMENT_NAME = 50;

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

struct SampleOffsets {
	size_t sampleSize;
	size_t startIndex;
   float minGain;
};

struct Instrument {
   int nota;
	char nombre[MAX_SIZE_INSTRUMENT_NAME];
   char nombreSample[MAX_SAMPLE_LAYERS][20] = {'\0'};
   SampleOffsets samples[MAX_SAMPLE_LAYERS]; // INDICA EL NÚMERO MÁXIMO DE CAPAS DEL SAMPLE <----------------
   int numberLayers = 0;
   float instrumentGain = 0.5f;
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
#ifdef ARM_ALLOW_MULTI_CORE
	: public CMultiCoreSupport
#endif
{
public:
   HyperNaturalSoundGenerator (CSoundBaseDevice &sound, CLogger &logger, CScheduler	&scheduler, CDeviceNameService &m_DeviceNameService, CSerialDevice &m_Serial, CTimer &m_Timer, CMemorySystem *pMemorySystem, CActLED m_ActLED);

   int samplesCheck();

   bool loadSamplesOnRAM();
   
   void loop();

	~HyperNaturalSoundGenerator (void);

   #ifndef ARM_ALLOW_MULTI_CORE
      boolean Initialize (void)	{ return TRUE; }
   #endif

   void Run(unsigned nCore);

private:
	// members ...
   CSoundBaseDevice &m_pSound;
   CLogger &m_Logger;
   CScheduler		m_Scheduler;
   CDeviceNameService &m_DeviceNameService;
   FATFS 			m_FileSystem;
   CSerialDevice m_Serial;
   CTimer &m_Timer;
   CActLED			m_ActLED;
   
   unsigned nQueueSizeFrames;

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

   int tmpVelocity = 1;

   // multi-threading
   volatile bool readyCore1 = false;
   volatile bool readyCore2 = false;
   volatile bool readyCore3 = false;

   // buffer for note and velocity
   volatile bool noteComplete = false;
   volatile u8 currentNote = 0;

   CSpinLock m_SpinLock;

   void TriggerVoice(u8 note, u8 velocity);
   void OnNeedData();
   static void OnNeedDataAdapter(void* ctx);
   void assignNoteToSample();
   void ProcessDirectory(const char *path, const char *parentPath);
   int extractNumber(const char *str);
   u8 determineLayerInstrument(u8 velocity, int numLayers);
   void getVelocityRange(int layer, int numLayers, int* vel_min, int* vel_max);
   static void CharReceivedHandler(u8 nChar, int nStatus, void *pParam);
   void sendInstrumentInfo();
};

#endif
