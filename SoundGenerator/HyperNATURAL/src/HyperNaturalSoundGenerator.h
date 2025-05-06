//
// myclass.h
//
#ifndef HYPERNATURALSOUNDGENERATOR
#define HYPERNATURALSOUNDGENERATOR

#include <circle/logger.h>
#include <circle/types.h>
#include <circle/sound/soundbasedevice.h>
#include <circle/devicenameservice.h>
#include <circle/fs/fat/fatfs.h>
#include <circle/util.h>
#include <circle/sched/scheduler.h>
#include <circle/serial.h>

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
	int sampleSize;
	int startIndex;
};

class HyperNaturalSoundGenerator
{
public:
   HyperNaturalSoundGenerator (CSoundBaseDevice &sound, CLogger &logger, CScheduler	&scheduler, CDeviceNameService &m_DeviceNameService, CSerialDevice &m_Serial);

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
   CFATFileSystem m_FileSystem;
   CSerialDevice m_Serial;
   
   unsigned nQueueSizeFrames;

   SampleOffsets sampleInfo[100];
   WavDirectory wavMemory[10];

   u8 *wavRoom;

   long totalSizeWavRoom;
   long usedSizeWavRoom;
   int totalSamples;
   int numberWavs;

   void writeWavData(unsigned nFrames, unsigned &remainingBytes, int sampleIndex, int &bufferChunk, u8 *wavRoom);

};

#endif
