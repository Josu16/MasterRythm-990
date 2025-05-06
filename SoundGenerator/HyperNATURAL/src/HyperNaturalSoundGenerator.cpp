//
// myclass.cpp
//
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

   // m_Logger.Write (FromKernel, LogPanic, "__Dispositivo de sonido configurado__"); // no se puede poner logger en la construcción de la clase, TODO: REvisar por que

	totalSizeWavRoom = 0;
	totalSamples = 0;
	usedSizeWavRoom = 0;
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

	return true;
}

void HyperNaturalSoundGenerator::loop() {
	//	PUESTA EN REPRODUCCIÓN DE LOS ARCHIVOS.

	m_Logger.Write (FromKernel, LogNotice, "REPRODUCIENDO muestra de inicialización, de tamaño... %d", sampleInfo[0].sampleSize);
	unsigned remainingBytesToRead = sampleInfo[0].sampleSize;
	int bufferChunk = 0; // ESTE LLEVA EL CONTROL DE LA LECTURA DE LA RAM EN LA MEMORIA WAV
	writeWavData (nQueueSizeFrames, remainingBytesToRead, 0, bufferChunk, wavRoom);

	// start sound device
	if (!m_pSound.Start ())
	{
		m_Logger.Write (FromKernel, LogPanic, "Cannot start sound device");
	}

	m_Logger.Write (FromKernel, LogNotice, "Se inicio el audio");

	u8 note;

	// TEMPORAL DE REPRODUCCIÓN
	while (1) {
		// Permanecer en una espera infinita a que llegue un nuevo sonido a reproducir.
		
		if (m_Serial.Read(&note, 1) > 0) {
			m_Logger.Write (FromKernel, LogNotice, "\t  Nota: %d", note);


			if (note == 36) {
				unsigned remainingBytesToRead = sampleInfo[0].sampleSize;
				int bufferChunk = 0; // ESTE LLEVA EL CONTROL DE LA LECTURA DE LA RAM EN LA MEMORIA WAV
				writeWavData (nQueueSizeFrames, remainingBytesToRead, 0, bufferChunk, wavRoom);
				while (m_pSound.IsActive() && remainingBytesToRead > 0) {
					writeWavData(nQueueSizeFrames - m_pSound.GetQueueFramesAvail(), remainingBytesToRead, 0, bufferChunk, wavRoom);
				}
			}
		}
		// m_Scheduler.MsSleep (200);
		// m_serial.Read()
	}
	// FIN DE TEMPORAL DE REPRODUCCIÓN
}

void HyperNaturalSoundGenerator::writeWavData(unsigned nFrames, unsigned &remainingBytes, int sampleIndex, int &bufferChunk, u8 *wavRoom) {
	// nFrames dice cuánto espacio (en frames) tengo en el buffer general de audio

	const unsigned nFramesPerWrite = 1024;

	// const unsigned sizeBufferCircle = nFramesPerWrite * WRITE_CHANNELS * TYPE_SIZE; // 4096 bytes

	// m_Logger.Write (FromKernel, LogNotice, "sizeBufferCircle %d", sizeBufferCircle);

	// u8 bufferToCircle[sizeBufferCircle]; // revisar si verdaderamente este buffer tan grande se está ocupando por completo.
	// el fragmento de datos que se mandará al audio gestionado por circle, es un buffer temporal.

	

	while (nFrames > 0 && remainingBytes > 0) {
		unsigned nWriteFrames = nFrames < nFramesPerWrite ? nFrames : nFramesPerWrite;
		// si el número de frames disponibles es menor que el número de frames por escritura,
		// se escribiá ese número menor, sino toma el tamaño máximo de frames que se pueden escribir. 

		// equivalencia de frames a bytes.
		unsigned nBytesByIter = nWriteFrames * WRITE_CHANNELS * TYPE_SIZE;
		
		// obtener datos de la microsd

		// saber el tamaño de BYTES que puedo leer de la ram
		unsigned nReadedBytes = remainingBytes < nBytesByIter ? remainingBytes : nBytesByIter;
		
		// unsigned int chunkWavFile = m_FileSystem.FileRead(file, bufferToCircle, nReadedBytes);
		// if (chunkWavFile == FS_ERROR) {
		// 	m_Logger.Write (FromKernel, LogPanic, "Error al leer fragmento");
		// }
		// bufferToCircle = 

		// SE TIENE QUE LLEVAR OTRO CONTADOR SOBRE LO QUE SE VA ITERANDO Y LO QUE NO
		// memcpy(bufferToCircle, wavRoom + sampleInfo[sampleIndex].startIndex + bufferChunk, nReadedBytes);


		// se terminaron de obtener los datos de la microsd

		int nResult = m_pSound.Write(wavRoom + sampleInfo[sampleIndex].startIndex + bufferChunk, nReadedBytes);
		if (nResult != (int)nReadedBytes) {
			m_Logger.Write(FromKernel, LogError, "no se pudo escribir el bloque") ;
		}
	
		nFrames -= nWriteFrames;
		remainingBytes -= nReadedBytes;
		bufferChunk += nReadedBytes;
		// m_Logger.Write(FromKernel, LogNotice, "Frames restantes en el buffer %d, bytes disponibles en la sd %d", nFrames, remainingBytes);
		// m_Scheduler.Yield ();		// ensure the VCHIQ tasks can run
	}
}

HyperNaturalSoundGenerator::~HyperNaturalSoundGenerator (void)
{
}