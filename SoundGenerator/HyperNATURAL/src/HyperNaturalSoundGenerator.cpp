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

#define DRIVE		"USB:"

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
static const char fromC1[] = "C1";
static const char fromC2[] = "C2";
static const char fromC3[] = "C3";

HyperNaturalSoundGenerator::HyperNaturalSoundGenerator (CSoundBaseDevice &sound, CLogger &logger, CScheduler &scheduler, CDeviceNameService	&m_DeviceNameService, CSerialDevice &m_Serial, CTimer &m_Timer, CMemorySystem *pMemorySystem)
:
CMultiCoreSupport (pMemorySystem), m_pSound(sound), m_Logger(logger), m_Scheduler(scheduler), m_DeviceNameService(m_DeviceNameService), m_Serial(m_Serial), m_Timer(m_Timer)
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
	if (f_mount (&m_FileSystem, DRIVE, 1) != FR_OK)
	{
		m_Logger.Write (FromKernel, LogPanic, "No se pudo montar la partición: %s", DRIVE);
	}
	else {
		m_Logger.Write (FromKernel, LogNotice, "SE MONTÓ la partición: %s", DRIVE);
	}

	// numberWavs = 0;

	ProcessDirectory(DRIVE, "");

	m_Logger.Write (FromKernel, LogNotice, "Número de instrumentos totales: %d", totalInstruments);

	// Reimprimir los archivos mostrados.

	for (int i = 0; i < totalInstruments; i++) {
		m_Logger.Write (FromKernel, LogNotice, "Instrumento: %s ", instruments[i].nombre);
		for (int j = 0; j < MAX_SAMPLE_LAYERS; j++) {
			m_Logger.Write (FromKernel, LogNotice, "Wav: %s ", instruments[i].nombreSample[j]);
		}
	}


	for (int i = 0; i < totalInstruments; i++) {
		for (int j = 0; j < MAX_SAMPLE_LAYERS; j++) {

			if (instruments[i].nombreSample[j][0] == '\0') continue; // para omitir espacios en blanco en las capas.

			FIL file;
			UINT bytesRead;
			FRESULT res;
			WAVHeader header;
			CString fullPath;

			m_Logger.Write (FromKernel, LogNotice, "Wav: %s ", instruments[i].nombreSample[j]);
			fullPath.Format("%s/%s/%s", DRIVE, instruments[i].nombre, instruments[i].nombreSample[j]);

			// unsigned sample = m_FileSystem.FileOpen(wavMemory[j].nombre);
			res = f_open(&file, fullPath , FA_READ);

			m_Logger.Write (FromKernel, LogNotice, "Abierto: %s ", instruments[i].nombreSample[j]);
			char wavHeader[44];

			// unsigned nEntryFile = m_FileSystem.FileRead(sample, wavHeader, 44);
			res = f_read(&file, wavHeader, 44, &bytesRead);
			if (res != FR_OK) {
				f_close(&file);
				numberWrongFiles ++;
				m_Logger.Write (FromKernel, LogError, "Error al abrir el header");
			}
			else if (bytesRead != 44)
			{
				f_close(&file);
				numberWrongFiles ++;
				m_Logger.Write(FromKernel, LogError, "El header del archivo WAV tiene un tamaño inesperado: %u bytes leídos, se esperaban %u bytes", bytesRead, (unsigned)sizeof(wavHeader));
			}
			else {
				// Opcional: Validar que la estructura tenga el tamaño correcto (44 bytes)
				if (sizeof(WAVHeader) != 44)
				{
					f_close(&file);
					numberWrongFiles++;
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
					f_close(&file);
					m_Logger.Write(FromKernel, LogError, "Tamaño de datos del WAV %s NO es múltiplo de 4: %u", instruments[i].nombreSample[j], header.subChunk2Size);
					return -1; // O maneja el error apropiadamente
				}
				else {
					m_Logger.Write(FromKernel, LogNotice, "Sample correcto");
				}
				//     // Aquí podrías procesar los datos de audio...
				totalSizeWavRoom += header.subChunk2Size;
				// sampleInfo[j].sampleSize = header.subChunk2Size;
				instruments[i].samples[j].sampleSize = header.subChunk2Size;
				totalSamples ++;
			}
			// cierre del archivo
			res = f_close(&file);
			if (res != FR_OK) {
					m_Logger.Write(FromKernel, LogPanic, "No se pudo cerrar el archivo '%s': FRESULT=%d", instruments[i].nombreSample[j], res);
					numberWrongFiles++;
			}
			
		}
	}

	loadSamplesOnRAM();

	f_mount(0, DRIVE, 0); // Desmontar la unidad

	return numberWrongFiles;
}

void HyperNaturalSoundGenerator::ProcessDirectory(const char *path, const char *parentPath) {
	DIR dir;
	FILINFO fno;
	FRESULT res;

	// Abrir directorio raíz
   res = f_opendir(&dir, path);
	if (res == FR_OK) {
		// Leer entradas una por una
		for (;;) {
			res = f_readdir(&dir, &fno);
			if (res != FR_OK || fno.fname[0] == 0) {
				// m_Logger.Write(FromKernel, LogWarning, "Fin de la lista");
				break; // Error o fin del directorio
			}

			// Omitir entradas ocultas y de sistema
			if (fno.fattrib & (AM_HID | AM_SYS)) continue;

			// Construir la ruta completa
			CString FullPath;
			FullPath.Format("%s/%s", path, fno.fname); // primera entrada del raíz
			m_Logger.Write(FromKernel, LogWarning, "Entrada en cuestion: %s", (const char *) FullPath);

			// Mostrar nombre y tipo
			if (fno.fattrib & AM_DIR) {
				m_Logger.Write(FromKernel, LogWarning, "Directorio %s", fno.fname);
				strcpy(instruments[totalInstruments].nombre, fno.fname); // NOMBRE DEL INSTRUMENTO
				ProcessDirectory((const char *) FullPath, fno.fname);
				totalInstruments ++;
			} else {
				m_Logger.Write (FromKernel, LogNotice, "Nombre del archivo %s ", fno.fname);
				int index = extractNumber(fno.fname);
				m_Logger.Write (FromKernel, LogNotice, "WAV WARDADO %s ", fno.fname);
				strcpy(instruments[totalInstruments].nombreSample[index-1], fno.fname);
			}
		}
		f_closedir(&dir); // Cerrar directorio
	}
	else {
		m_Logger.Write (FromKernel, LogPanic, "ERROR AL MONTAR EL DIR %s: ", res);
	}
}

bool HyperNaturalSoundGenerator::loadSamplesOnRAM() {

	// cargar los samples en RAM

	m_Logger.Write (FromKernel, LogNotice, "Tamaño total reservado: %d", totalSizeWavRoom);
	wavRoom = new u8[totalSizeWavRoom];
	// int currentWavPosition = 0;

	for (int i = 0; i < totalInstruments; i++ ) { 
		for (int j = 0; j < MAX_SAMPLE_LAYERS; j++) {
			if (instruments[i].nombreSample[j][0] == '\0') continue;

			FIL file;
			UINT bytesRead;
			FRESULT res;
			CString fullPath;

			fullPath.Format("%s/%s/%s", DRIVE, instruments[i].nombre, instruments[i].nombreSample[j]);
			// unsigned sample = m_FileSystem.FileOpen(wavMemory[wavIndex].nombre);

			res = f_open(&file, fullPath , FA_READ);

			char wavHeader[44];
			// unsigned nEntryFile = m_FileSystem.FileRead(sample, wavHeader, 44);
			res = f_read(&file, wavHeader, 44, &bytesRead);

			if (res != FR_OK) {
				f_close(&file);
				m_Logger.Write (FromKernel, LogError, "Error al abrir el header");
			}

			// Comienza la lectura desde la USB.
			// int remainingBytes = sampleInfo[wavIndex].sampleSize;
			int remainingBytes = instruments[i].samples[j].sampleSize;
			int maxBlockRead = 1000000;
			// int indexReadingSample = 0;
			while (remainingBytes > 0) {
				int nBytesToRead = remainingBytes > maxBlockRead ? maxBlockRead : remainingBytes;
				u8 subchunk2[nBytesToRead];
				// unsigned int chunkWavFile = m_FileSystem.FileRead(sample, subchunk2, nBytesToRead);
				res = f_read(&file, subchunk2, nBytesToRead, &bytesRead);
				if (res != FR_OK) {
					m_Logger.Write (FromKernel, LogPanic, "Error al leer fragmento de audio");
				}
				else {
					// COMIENZA CARGA DE ARCHIVOS

					// sampleInfo[wavIndex].startIndex = usedSizeWavRoom;
					instruments[i].samples[j].startIndex = usedSizeWavRoom;
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
			m_Logger.Write (FromKernel, LogNotice, " -Índice de inicio %d", instruments[i].samples[j].startIndex);
			m_Logger.Write (FromKernel, LogNotice, " -Tamaño del sample %d", instruments[i].samples[j].sampleSize);
			// cierre del archivo
			res = f_close(&file);
			if (res != FR_OK) {
					m_Logger.Write(FromKernel, LogPanic, "No se pudo cerrar el archivo '%s': FRESULT=%d", instruments[i].nombreSample[j], res);
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
	
	m_Logger.Write(FromKernel, LogNotice, "Despertando core 1");
	readyCore1 = true;

	// unsigned nCelsius = CCPUThrottle::Get ()->GetTemperature ();
	// 		m_Logger.Write (fromC2, LogNotice, "Temperatura actual %d", nCelsius);
		
	// u8 note;
	// // u8 velocity;
	// while (true) {
	// 	// if (m_Serial.Read(&note, 1) > 0 && m_Serial.Read(&velocity, 1) > 0) {
	// 	// 		TriggerVoice(note, velocity);
	// 	// }
	// 	if (m_Serial.Read(&note, 1) > 0) {
	// 		tmpVelocity ++;

	// 		if (tmpVelocity >= 127 ) {
	// 			tmpVelocity = 1;
	// 		}


	// 		TriggerVoice(note, tmpVelocity);
	// 	}
	// 	// espera ligera hasta próxima IRQ
	// 	// Arch::Halt();
	// 	// m_Scheduler.Yield();
	// }
	// while(1) {
	// 	m_Scheduler.Yield();
	// }
}

void HyperNaturalSoundGenerator::OnNeedDataAdapter(void* ctx)
{
	static_cast<HyperNaturalSoundGenerator*>(ctx)->OnNeedData();
}

u8 HyperNaturalSoundGenerator::determineLayerInstrument(u8 velocity) {
    return (velocity * 6) / 128;
}

void HyperNaturalSoundGenerator::OnNeedData()
{
	// unsigned currentTime = m_Timer.GetTicks();  // Tiempo actual
// 1. Activar todas las notas pendientes
	m_SpinLock.Acquire ();
	while (pendingTail != pendingHead) {
		if (pendingNotes[pendingTail].used) {
			u8 note = pendingNotes[pendingTail].note;
			u8 velocity = pendingNotes[pendingTail].velocity;
			// unsigned arrivalTime = pendingNotes[pendingTail].arrivalTime;
			// unsigned delay = currentTime - arrivalTime;  // Retraso en ticks o ms
			// m_Logger.Write(FromKernel, LogDebug, "Nota %u con retraso %u ms", note, delay);

			// Buscar una voz libre y activarla
			for (int i = 0; i < MAX_VOICES; ++i) {
					if (!m_Voices[i].active) {
						int idx = m_NoteToSample[note];
						if (idx >= 0) {
							// m_Voices[i].sample = &sampleInfo[idx]
							u8 layer = determineLayerInstrument(velocity);
							m_Voices[i].sample = &instruments[idx].samples[layer];
							m_Voices[i].pos = 0;
							m_Voices[i].gain = 0.05f;
							m_Voices[i].active = true;
						}
						break;
					}
			}
			pendingNotes[pendingTail].used = false;  // Liberar entrada
		}
		pendingTail = (pendingTail + 1) % MAX_PENDING_NOTES;
	}
	m_SpinLock.Release ();


	// AQUÍ COMIENZA EL FRAGMENTO NO SINCRONIZADO CON ISR, PARA REGRESAR, ELIMINE TODO EL CÓDIGO DE ARRIBA DE LA FUNCIÓN ISR.
	static s16 mixBuf[CHUNK_SIZE * WRITE_CHANNELS] = {0};

	// Reiniciar el buffer de mezcla a cero
	memset(mixBuf, 0, sizeof(mixBuf)); // imprescindible inicialización, para poder limpiar el buffer.

	// Procesar cada voz activa
	for (int i = 0; i < MAX_VOICES; ++i) {
		m_SpinLock.Acquire ();
		 if (m_Voices[i].active) {
			  Voice& v = m_Voices[i];
			  m_SpinLock.Release ();
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
		 else 
		 	m_SpinLock.Release ();
	}

	// Escribir el buffer mezclado al dispositivo de sonido
	int nResult = m_pSound.Write(reinterpret_cast<const u8*>(mixBuf), sizeof(mixBuf));
	if (nResult != (int) sizeof(mixBuf)) {
		m_Logger.Write(FromKernel, LogError, "no se pudo escribir el bloque") ;
	}
}

void HyperNaturalSoundGenerator::TriggerVoice(u8 note, u8 velocity)
{
	// const char *mensaje = "\n";
	// m_Logger.Write(fromC1, LogWarning, "not %d vel %d", note, velocity);
	// m_Logger.Write(fromC1, LogWarning, "not %d ", note);
	// m_Serial.Write(mensaje, strlen(mensaje));
	

	// DisableInterrupts();
	int idx = m_NoteToSample[note]; // note number
	if (idx < 0) {
		// EnableInterrupts();
		return;   // no hay sample para esta nota
	}

	m_SpinLock.Acquire ();
	int next = (pendingHead + 1) % MAX_PENDING_NOTES;
	if (next != pendingTail) {  // Verifica que la cola no esté llena
		// pendingNotes[pendingHead].arrivalTime = m_Timer.GetTicks();  // Registrar tiempo de llegada
		pendingNotes[pendingHead].note = note;
		pendingNotes[pendingHead].velocity = velocity;
		pendingNotes[pendingHead].used = true;
		pendingHead = next;
	} else {
		m_Logger.Write(FromKernel, LogWarning, "Cola de notas llena");
	}
	m_SpinLock.Release ();

	// USAR SOLO CON FINES DE DEPURACIÓN
	// ayuda a determinar la polifonia que se está consumiendo
	// sin embargo aparentemente la escritura serial provoca problemas de sincronización con la 
	// ISR del driver de audio.
	// for (int i=0; i<MAX_VOICES; i++) { 
	// 	if (m_Voices[i].active) {
	// 		m_Logger.Write(FromKernel, LogNotice, "Voz %d esta %d", i, m_Voices[i].active);
	// 	}
	// }

	// VERSIÓN NO SINCRONIZADA CON ISR
   //  // busca una ranura libre
   //  for (int i = 0; i < MAX_VOICES; ++i) {
	// 	// m_Logger.Write(FromKernel, LogNotice, "Voz %d esta %d", i, m_Voices[i].active);
	// 	if (!m_Voices[i].active) {
	// 		m_Voices[i].sample = &sampleInfo[idx];
	// 		m_Voices[i].pos    = 0;
	// 		m_Voices[i].gain   = 0.2f;
	// 		m_Voices[i].active = true;

	// 		// EnableInterrupts();

	// 		// m_Logger.Write(FromKernel, LogNotice, "sampleInfo.startIndex %d", sampleInfo->startIndex);
	// 		// m_Logger.Write(FromKernel, LogNotice, "sampleInfo.sampleSize %d", sampleInfo->sampleSize);
	// 		return;
	// 	}
   //  }
	// //  EnableInterrupts();
	// //  m_Logger.Write(FromKernel, LogNotice, "Nota activa %d", note);
   //  // opcional: si está lleno, podrías robar la voz más antigua o descartarla
}

void HyperNaturalSoundGenerator::assignNoteToSample() {
	m_NoteToSample[36] = 0;   // nota 36 dispara sampleInfo[0]
	m_NoteToSample[42] = 2;   // nota 38 dispara sampleInfo[6]
	m_NoteToSample[35] = 3;   // nota 38 dispara sampleInfo[6]
	m_NoteToSample[56] = 1;   // nota 38 dispara sampleInfo[6]
}

// Extrae el primer número de una cadena como entero
// Devuelve -1 si no se encuentra un número
int HyperNaturalSoundGenerator::extractNumber(const char *str) {
    const char *p = str;
    int number = 0;
    bool foundDigit = false;

    // Avanzar hasta encontrar un dígito (carácter entre '0' y '9')
    while (*p && (*p < '0' || *p > '9')) {
        p++;
    }

    // Si no se encontraron dígitos, devolver -1
    if (!*p) {
        return -1;
    }

    // Construir el número dígito por dígito
    while (*p >= '0' && *p <= '9') {
        foundDigit = true;
        number = number * 10 + (*p - '0'); // Convertir carácter a valor numérico
        p++;
    }

    return foundDigit ? number : -1;
}

HyperNaturalSoundGenerator::~HyperNaturalSoundGenerator (void)
{
}


/* 
La forma de trabajo de esto es: la biblioteca subyacente inicia el método run en cada core físico
por separado, e, mparámetro nCore identifíca qué núcleo está ejecutándose en el código.

*/
void HyperNaturalSoundGenerator::Run (unsigned nCore)
{
// #ifdef ARM_ALLOW_MULTI_CORE

	switch (nCore)
	{
	case 0:
		// m_Scheduler.Sleep ();
		// Calculate (-2.0, 1.0, -1.0, -0.5, MAX_ITERATION, 0, nQuarterHeight);
		m_Logger.Write (FromKernel, LogNotice, "Nucleo 0");
		break;

	case 1:
		u8 note, velocity;
		while (1) {
			if (readyCore1) {
				if (m_Serial.Read(&note, 1) > 0) {
					// tmpVelocity ++;

					// if (tmpVelocity >= 127 ) {
					// 	tmpVelocity = 1;
					// }

					// TriggerVoice(note, tmpVelocity);
					velocity = 100;
					TriggerVoice(note, velocity);
				}
			}
		}
		break;

	case 2:
		// while (1) {
			// m_Scheduler.Sleep (5000);
			m_Logger.Write (FromKernel, LogNotice, "Nucleo 2");
		// }
		// m_Scheduler.Sleep ();
		// Calculate (-2.0, 1.0, 0.0, 0.5, MAX_ITERATION, nQuarterHeight*2, nQuarterHeight);
		break;

	case 3:
		// while (1) {
			// m_Scheduler.Sleep (2500);
			m_Logger.Write (FromKernel, LogNotice, "Nucleo 3");
		// }
		// Calculate (-2.0, 1.0, 0.5, 1.0, MAX_ITERATION, nQuarterHeight*3, nQuarterHeight);
		break;
	}
// #else
// 	// Calculate (-2.0, 1.0, -1.0, 1.0, MAX_ITERATION, 0, m_pScreen->GetHeight ());
// #endif
}