//
// myclass.cpp
//

#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#include "HyperNaturalSoundGenerator.h"
#include "config.h"

#define PARTITION	"umsd1-1"
#define FILENAME	"circle.txt"
#define NSAMPLES 3

#ifdef USE_VCHIQ_SOUND
	#include <vc4/sound/vchiqsoundbasedevice.h>
#endif

#if WRITE_FORMAT == 0
	#define FORMAT		SoundFormatUnsigned8
	#define TYPE		u8
	#define TYPE_SIZE	sizeof (u8)
	#define FACTOR		((1 << 7)-1)
	#define NULL_LEVEL	(1 << 7)
#elif WRITE_FORMAT == 1
	#define FORMAT		SoundFormatSigned16
	#define TYPE		s16
	#define TYPE_SIZE	sizeof (s16)
	#define FACTOR		((1 << 15)-1)
	#define NULL_LEVEL	0
#elif WRITE_FORMAT == 2
	#define FORMAT		SoundFormatSigned24
	#define TYPE		s32
	#define TYPE_SIZE	(sizeof (u8)*3)
	#define FACTOR		((1 << 23)-1)
	#define NULL_LEVEL	0
#endif

static const char FromKernel[] = "HNSoundGenerator";

HyperNaturalSoundGenerator::HyperNaturalSoundGenerator (CSoundBaseDevice &sound, CLogger &logger, CScheduler &scheduler, CDeviceNameService	&m_DeviceNameService, CSerialDevice &m_Serial)
:
m_pSound(sound), m_Logger(logger), m_Scheduler(scheduler), m_DeviceNameService(m_DeviceNameService), m_Serial(m_Serial)
{
   // configure sound device
	if (!m_pSound.AllocateQueue (QUEUE_SIZE_MSECS)) // Creación o asignación de tamaño de buffer de audio en MS (100)
	{
		m_Logger.Write (FromKernel, LogPanic, "No se pudo ubicar la cola de sonido");
	}

	m_pSound.SetWriteFormat (FORMAT, WRITE_CHANNELS); // debe determinar cómo va a enviar los paquetes de bytes (en fn a los canales y bit depth)

	nQueueSizeFrames = m_pSound.GetQueueSizeFrames (); // se obtiene el tamaño del buffer pero en frames

   m_Logger.Write (FromKernel, LogNotice, "__Dispositivo de sonido configurado__"); // no se puede poner logger en la construcción de la clase, TODO: REvisar por que

	totalSizeWavRoom = 0;
	totalSamples = 0;
	usedSizeWavRoom = 0;

	m_pSound.RegisterNeedDataCallback(
		&HyperNaturalSoundGenerator::OnNeedDataAdapter,
		this
  	);

  	//   inicializa todo a “no asignado”
	for(int i = 0; i < NUM_NOTES; ++i)
		m_NoteToSample[i] = -1;
}

int HyperNaturalSoundGenerator::samplesCheck() {
   int numberWrongFiles = 0;

   // APERTURA DE ARCHIVOS
	m_Logger.Write (FromKernel, LogNotice, "Compile time: " __DATE__ " " __TIME__);

	// Mount file system
	CDevice *pPartition = m_DeviceNameService.GetDevice (PARTITION, TRUE);
	if (pPartition == 0)
	{
		m_Logger.Write (FromKernel, LogPanic, "No se encontro la particion: %s", PARTITION);
	}

	if (!m_FileSystem.Mount (pPartition))
	{
		m_Logger.Write (FromKernel, LogPanic, "No se pudo montar la particion: %s", PARTITION);
	}


   // MOSTRAR EL DIRECTORIO ACTUAL
	numberWavs = 0;

	TDirentry Direntry;
	TFindCurrentEntry CurrentEntry;
	unsigned nEntry = m_FileSystem.RootFindFirst (&Direntry, &CurrentEntry);
	for (unsigned i = 0; nEntry != 0; i++) // iterar hasta que el número de entrads sea igual a cero.
	{
		if (!(Direntry.nAttributes & FS_ATTRIB_SYSTEM)) // Si no es un archivo de sistema
		{
			CString FileName;
			// FileName.Format ("%-10s", Direntry.chTitle);
			m_Logger.Write (FromKernel, LogNotice, "Nombre del archivo %s ", (const char *) Direntry.chTitle);
			strcpy(wavMemory[numberWavs].nombre, Direntry.chTitle);
			numberWavs++;

			// Insertar en el mapa
			// fileMap[my_custom_index] = std::string((const char*)FileName); cencerro6.wav
			// m_Scheduler.MsSleep (100);
			// const char *mensaje =   FileName;
			// m_Serial.Write(mensaje, strlen(mensaje));
		}

		nEntry = m_FileSystem.RootFindNext (&Direntry, &CurrentEntry);
	}

	// Reimprimir los archivos mostrados.
	for (int i = 0; i<numberWavs; i++ ) {
		m_Logger.Write (FromKernel, LogNotice, "Wav: %s ", wavMemory[i].nombre);
	} 

	// VERIFICAR FORMATO DE ARCHIVOS.

	for (int j = 0; j<numberWavs; j++ ) { 
		WAVHeader header;

		unsigned sample = m_FileSystem.FileOpen(wavMemory[j].nombre);

		if (sample == 0) { // será cero cuando no haya más archivos en el directorio
			m_Logger.Write (FromKernel, LogPanic, "No se pudo abrir: %s ", wavMemory[j].nombre);
		}
		else {
			m_Logger.Write (FromKernel, LogNotice, "Abierto: %s ", wavMemory[j].nombre);
			char wavHeader[44];
			unsigned nEntryFile = m_FileSystem.FileRead(sample, wavHeader, 44);
			if (nEntryFile == FS_ERROR) {
				m_Logger.Write (FromKernel, LogError, "Error al abrir el header");
			}
			else if (nEntryFile != sizeof(wavHeader))
			{
				m_Logger.Write(FromKernel, LogError, "El header del archivo WAV tiene un tamaño inesperado: %u bytes leídos, se esperaban %u bytes", nEntryFile, (unsigned)sizeof(wavHeader));
			}
			else {
				// Opcional: Validar que la estructura tenga el tamaño correcto (44 bytes)
				if (sizeof(WAVHeader) != 44)
				{
					m_Logger.Write(FromKernel, LogError, "El tamaño de la estructura WAVHeader es %u bytes, se esperaba 44 bytes", (unsigned)sizeof(WAVHeader));
				}

				// Mapear los datos leídos en nuestra estructura
				
				memcpy(&header, wavHeader, sizeof(header));

				// Convertir los campos de 4 bytes en cadenas nulas terminadas
				char chunkID[5], format[5], subChunk1ID[5], subChunk2ID[5];
				memcpy(chunkID, header.chunkID, 4);
				chunkID[4] = '\0';
				memcpy(format, header.format, 4);
				format[4] = '\0';
				memcpy(subChunk1ID, header.subChunk1ID, 4);
				subChunk1ID[4] = '\0';
				memcpy(subChunk2ID, header.subChunk2ID, 4);
				subChunk2ID[4] = '\0';

				// Imprimir toda la información del header
				m_Logger.Write(FromKernel, LogNotice, "Información completa del header WAV:");
				m_Logger.Write(FromKernel, LogNotice, "  Chunk ID: %s", header.chunkID);
				m_Logger.Write(FromKernel, LogNotice, "  Chunk Size: %u", header.chunkSize);
				m_Logger.Write(FromKernel, LogNotice, "  Format: %s", header.format);
				m_Logger.Write(FromKernel, LogNotice, "  Subchunk1 ID: %s", header.subChunk1ID);
				m_Logger.Write(FromKernel, LogNotice, "  Subchunk1 Size: %u", header.subChunk1Size);
				m_Logger.Write(FromKernel, LogNotice, "  Audio Format: %u", header.audioFormat);
				m_Logger.Write(FromKernel, LogNotice, "  Número de canales: %u", header.numChannels);
				m_Logger.Write(FromKernel, LogNotice, "  Frecuencia de muestreo: %u", header.sampleRate);
				m_Logger.Write(FromKernel, LogNotice, "  Byte Rate: %u", header.byteRate);
				m_Logger.Write(FromKernel, LogNotice, "  Block Align: %u", header.blockAlign);
				m_Logger.Write(FromKernel, LogNotice, "  Bits per Sample: %u", header.bitsPerSample);
				m_Logger.Write(FromKernel, LogNotice, "  Subchunk2 ID: %s", header.subChunk2ID);
				m_Logger.Write(FromKernel, LogNotice, "  ----Subchunk2 Size: %u", header.subChunk2Size);

				// Validar que el contenido del header tenga el formato esperado
				if (strncmp(header.chunkID, "RIFF", 4) != 0)
				{
					m_Logger.Write(FromKernel, LogError, "Error: Chunk ID no es 'RIFF'");
				}
				if (strncmp(header.format, "WAVE", 4) != 0)
				{
					m_Logger.Write(FromKernel, LogError, "Error: Format no es 'WAVE'");
				}
				if (header.subChunk1Size != 16)
				{
					m_Logger.Write(FromKernel, LogError, "Error: Subchunk1 Size es %u, se esperaba 16 para PCM", header.subChunk1Size);
				}
				if (header.audioFormat != 1)
				{
					m_Logger.Write(FromKernel, LogError, "Error: Audio Format es %u, se esperaba 1 para PCM", header.audioFormat);
				}	

				
				// Verificación de alineación de datos
            if (header.subChunk2Size % 4 != 0) {
                m_Logger.Write(FromKernel, LogError, "Tamaño de datos del WAV %s NO es múltiplo de 4: %u", wavMemory[j].nombre, header.subChunk2Size);
                m_FileSystem.FileClose(sample);
                return -1; // O maneja el error apropiadamente
            }
				else {
					 m_Logger.Write(FromKernel, LogNotice, "Audio correcto");
				}
				//     // Aquí podrías procesar los datos de audio...
				totalSizeWavRoom += header.subChunk2Size;
				sampleInfo[j].sampleSize = header.subChunk2Size;
				totalSamples ++;
			}
			// cierre del archivo
			if (!m_FileSystem.FileClose (sample)) {
				m_Logger.Write (FromKernel, LogPanic, "No se pudo cerrar el archivo");
			}
		}
	}


	return numberWrongFiles;
}


bool HyperNaturalSoundGenerator::loadSamplesOnRAM() {
	// cargar los samples en RAM

	m_Logger.Write (FromKernel, LogNotice, "Tamaño total reservado: %d", totalSizeWavRoom);
	wavRoom = new u8[totalSizeWavRoom];
	// int currentWavPosition = 0;

	for (int wavIndex = 0; wavIndex < numberWavs; wavIndex++ ) { 
		unsigned sample = m_FileSystem.FileOpen(wavMemory[wavIndex].nombre);

		if (sample == 0) { // será cero cuando no haya más archivos en el directorio
			m_Logger.Write (FromKernel, LogPanic, "No se pudo abrir: %s ", wavMemory[wavIndex].nombre);
		}
		else {
			char wavHeader[44];
			unsigned nEntryFile = m_FileSystem.FileRead(sample, wavHeader, 44);
			if (nEntryFile == FS_ERROR) {
				m_Logger.Write (FromKernel, LogError, "Error al desechar el header");
			}

			// Comienza la lectura desde la USB.
			int remainingBytes = sampleInfo[wavIndex].sampleSize;
			int maxBlockRead = 1000000;
			// int indexReadingSample = 0;
			while (remainingBytes > 0) {
				int nBytesToRead = remainingBytes > maxBlockRead ? maxBlockRead : remainingBytes;
				u8 subchunk2[nBytesToRead];
				unsigned int chunkWavFile = m_FileSystem.FileRead(sample, subchunk2, nBytesToRead);
				if (chunkWavFile == FS_ERROR) {
					m_Logger.Write (FromKernel, LogPanic, "Error al leer fragmento de audio");
				}
				else {
					// COMIENZA CARGA DE ARCHIVOS

					sampleInfo[wavIndex].startIndex = usedSizeWavRoom;
					for (int loadWavIndex = 0; loadWavIndex < nBytesToRead; loadWavIndex++) {
						wavRoom[usedSizeWavRoom] = subchunk2[loadWavIndex];
						usedSizeWavRoom ++;
					}

					// FINALIZA CARGA DE ARCHIVOS
					// m_Logger.Write (FromKernel, LogNotice, "Se leyeron: %d", chunkWavFile);
				}
				remainingBytes -= nBytesToRead;
			}
			
			m_Logger.Write (FromKernel, LogNotice, "\t Aarchivo leído. Memoria de wav usada acumulada %d", usedSizeWavRoom);
			m_Logger.Write (FromKernel, LogNotice, " -Índice de inicio %d", sampleInfo[wavIndex].startIndex);
			m_Logger.Write (FromKernel, LogNotice, " -Tamaño del sample %d", sampleInfo[wavIndex].sampleSize);
			// cierre del archivo
			if (!m_FileSystem.FileClose (sample)) {
				m_Logger.Write (FromKernel, LogPanic, "No se pudo cerrar el archivo");
			}
		}
	}
	assignNoteToSample();
	return true;
}

void HyperNaturalSoundGenerator::loop() {

	// 1) Primear unos cuantos frames antes de arrancar
	// Silence buffer estático de CHUNK_SIZE frames:
	static s16 silenceBuf[CHUNK_SIZE * WRITE_CHANNELS] = {0};

	// Total de frames que caben en la cola:
	unsigned totalFrames = nQueueSizeFrames;

	while (totalFrames > 0)
	{
		unsigned block = totalFrames < CHUNK_SIZE
								? totalFrames
								: CHUNK_SIZE;
		unsigned bytes = block * WRITE_CHANNELS * TYPE_SIZE;

		int written = m_pSound.Write(
			reinterpret_cast<const u8*>(silenceBuf),
			bytes
		);
		if (written != (int)bytes)
			m_Logger.Write(FromKernel, LogError,
								"Primeo: sólo escribió %d de %u bytes",
								written, bytes);

		totalFrames -= block;
	}

	// 2) Arrancar el driver
	if (!m_pSound.Start())
		m_Logger.Write(FromKernel, LogPanic, "No se pudo iniciar el dispositivo de audio");
	else {
		m_Logger.Write(FromKernel, LogNotice, "Audio iniciado");
		unsigned framesAvail = m_pSound.GetQueueFramesAvail();
		m_Logger.Write(FromKernel, LogNotice, "-frames iniciales: %d", framesAvail);
	}
		
	u8 note;
	while (true) {
		if (m_Serial.Read(&note, 1) > 0) {
				TriggerVoice(note);
		}
		// espera ligera hasta próxima IRQ
		// Arch::Halt();
		// m_Scheduler.Yield();
	}
}

void HyperNaturalSoundGenerator::OnNeedDataAdapter(void* ctx)
{
	static_cast<HyperNaturalSoundGenerator*>(ctx)->OnNeedData();
}

void HyperNaturalSoundGenerator::OnNeedData()
{
	// unsigned framesAvail = m_pSound.GetQueueFramesAvail();
   //  m_Logger.Write(FromKernel, LogDebug, "Frames disponibles: %u", framesAvail);
   //  if (framesAvail < CHUNK_SIZE) {
   //      m_Logger.Write(FromKernel, LogWarning, "Buffer bajo: %u frames disponibles", framesAvail);
   //  }
	// Buffer de mezcla para un chunk: 512 frames * 2 canales * 2 bytes por muestra
	static s16 mixBuf[CHUNK_SIZE * WRITE_CHANNELS] = {0};

	// Reiniciar el buffer de mezcla a cero
	memset(mixBuf, 0, sizeof(mixBuf)); // imprescindible inicialización, para poder limpiar el buffer.

	// Procesar cada voz activa
	for (int i = 0; i < MAX_VOICES; ++i) {
		 if (m_Voices[i].active) {
			  Voice& v = m_Voices[i];
			  const SampleOffsets* s = v.sample;

			  // Calcular cuántos frames procesar para este chunk
			  size_t framesToProcess = CHUNK_SIZE;

			  while (framesToProcess > 0) {
					// Calcular frames restantes en el sample
					size_t bytesPerFrame = 4; // 2 canales * 2 bytes por muestra
					size_t totalFramesInWav = s->sampleSize / bytesPerFrame;
					size_t framesPlayed = v.pos / bytesPerFrame;
					size_t framesRemaining = totalFramesInWav - framesPlayed;

					if (framesRemaining == 0) {
						 v.active = false; // El sample ha terminado
						 break;
					}

					// Determinar cuántos frames copiar en esta iteración
					int framesToCopy = (framesToProcess < framesRemaining) ? framesToProcess : framesRemaining;

					for (int j = 0; j < framesToCopy; ++j) {
						 // Obtener el frame estéreo desde wavRoom
						size_t srcIdx = s->startIndex + v.pos;

						if (v.pos + bytesPerFrame > s->sampleSize) {
							m_Logger.Write(FromKernel, LogError, "Índice de acceso fuera de rango: %u", srcIdx);
							v.active = false;
							break;
						}

						// s16 leftSample = static_cast<s16>((wavRoom[srcIdx + 1] << 8) | wavRoom[srcIdx]);
						// s16 rightSample = static_cast<s16>((wavRoom[srcIdx + 3] << 8) | wavRoom[srcIdx + 2]);
						s16* sampleData = reinterpret_cast<s16*>(wavRoom + s->startIndex + v.pos);
						s16 leftSample = sampleData[0];
						s16 rightSample = sampleData[1];

						// Mezclar en los canales correspondientes
						int outIdx = (CHUNK_SIZE - framesToProcess + j) * WRITE_CHANNELS;
						mixBuf[outIdx] += static_cast<s16>(leftSample * v.gain);     // Canal izquierdo
						mixBuf[outIdx + 1] += static_cast<s16>(rightSample * v.gain); // Canal derecho

						// Avanzar la posición en bytes
						v.pos += bytesPerFrame;
					}

					framesToProcess -= framesToCopy;
			  }
		 }
	}

	// Escribir el buffer mezclado al dispositivo de sonido
	int nResult = m_pSound.Write(reinterpret_cast<const u8*>(mixBuf), sizeof(mixBuf));
	if (nResult != (int) sizeof(mixBuf)) {
		m_Logger.Write(FromKernel, LogError, "no se pudo escribir el bloque") ;
	}
}

void HyperNaturalSoundGenerator::TriggerVoice(u8 note)
{
	// DisableInterrupts();
	int idx = m_NoteToSample[note];
		if (idx < 0) {
			// EnableInterrupts();
			return;   // no hay sample para esta nota
		}

    // busca una ranura libre
    for (int i = 0; i < MAX_VOICES; ++i) {
		// m_Logger.Write(FromKernel, LogNotice, "Voz %d esta %d", i, m_Voices[i].active);
		if (!m_Voices[i].active) {
			m_Voices[i].sample = &sampleInfo[idx];
			m_Voices[i].pos    = 0;
			m_Voices[i].gain   = 0.2f;
			m_Voices[i].active = true;

			// EnableInterrupts();

			// m_Logger.Write(FromKernel, LogNotice, "sampleInfo.startIndex %d", sampleInfo->startIndex);
			// m_Logger.Write(FromKernel, LogNotice, "sampleInfo.sampleSize %d", sampleInfo->sampleSize);
			return;
		}
    }
	//  EnableInterrupts();
	//  m_Logger.Write(FromKernel, LogNotice, "Nota activa %d", note);
    // opcional: si está lleno, podrías robar la voz más antigua o descartarla
}

void HyperNaturalSoundGenerator::assignNoteToSample() {
	m_NoteToSample[36] = 3;   // nota 36 dispara sampleInfo[0]
	m_NoteToSample[42] = 9;   // nota 38 dispara sampleInfo[6]
	m_NoteToSample[56] = 6;   // nota 38 dispara sampleInfo[6]
	// m_NoteToSample[] = 9;   // nota 38 dispara sampleInfo[6]
}

HyperNaturalSoundGenerator::~HyperNaturalSoundGenerator (void)
{
}