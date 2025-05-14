//
// kernel.cpp
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2014-2024  R. Stange <rsta2@o2online.de>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include "kernel.h"
#include "config.h"
#include "HyperNaturalSoundGenerator.h"
#include <circle/sound/pwmsoundbasedevice.h>
#include <circle/sound/i2ssoundbasedevice.h>
#include <circle/sound/hdmisoundbasedevice.h>
#include <circle/sound/usbsoundbasedevice.h>
#include <circle/machineinfo.h>
#include <circle/util.h>
#include <circle/memory.h>
#include <assert.h>
#include <circle/synchronize.h>

static const char FromKernel[] = "kernel";

HyperNaturalSoundGenerator *soundGenerator;

CKernel::CKernel (void)
:	m_Screen (m_Options.GetWidth (), m_Options.GetHeight ()),
	m_Timer (&m_Interrupt),
	m_Logger (m_Options.GetLogLevel (), &m_Timer),
	m_I2CMaster (CMachineInfo::Get ()->GetDevice (DeviceI2CMaster), TRUE),
	m_USBHCI (&m_Interrupt, &m_Timer, FALSE),
#ifdef USE_VCHIQ_SOUND
	m_VCHIQ (CMemorySystem::Get (), &m_Interrupt),
#endif
	m_pSound (0)
	// m_VFO (&m_LFO)		// LFO modulates the VFO
{
	m_ActLED.Blink (5);	// show we are alive
}

CKernel::~CKernel (void)
{
}

boolean CKernel::Initialize (void)
{
	boolean bOK = TRUE;

	if (bOK)
	{
		bOK = m_Screen.Initialize ();
	}

	if (bOK)
	{
		// bOK = m_Serial.Initialize (115200);
		m_Serial.Initialize(115200, 8, 1, CSerialDevice::ParityNone); // ParityNone = 0
	}

	if (bOK)
	{
		// CDevice *pTarget = m_DeviceNameService.GetDevice (m_Options.GetLogDevice (), FALSE);
		// if (pTarget == 0)
		// {
		// 	pTarget = &m_Screen;
		// }

		bOK = m_Logger.Initialize (&m_Serial);
	}

	if (bOK)
	{
		bOK = m_Interrupt.Initialize ();
	}

	if (bOK)
	{
		bOK = m_Timer.Initialize ();
	}

	if (bOK)
	{
		bOK = m_I2CMaster.Initialize ();
	}

	if (bOK)
	{
		bOK = m_USBHCI.Initialize ();
	}

#ifdef USE_VCHIQ_SOUND
	if (bOK)
	{
		bOK = m_VCHIQ.Initialize ();
	}
#endif

	return bOK;
}

TShutdownMode CKernel::Run (void)
{
	m_Logger.Write (FromKernel, LogNotice, "Compile time: " __DATE__ " " __TIME__);

	// select the sound device
	const char *pSoundDevice = m_Options.GetSoundDevice ();
	if (strcmp (pSoundDevice, "sndpwm") == 0)
	{
		m_pSound = new CPWMSoundBaseDevice (&m_Interrupt, SAMPLE_RATE, CHUNK_SIZE);
	}
	else if (strcmp (pSoundDevice, "sndi2s") == 0)
	{
		m_pSound = new CI2SSoundBaseDevice (&m_Interrupt, SAMPLE_RATE, CHUNK_SIZE, FALSE,
						    &m_I2CMaster, DAC_I2C_ADDRESS);
	}
	else if (strcmp (pSoundDevice, "sndhdmi") == 0)
	{
		m_pSound = new CHDMISoundBaseDevice (&m_Interrupt, SAMPLE_RATE, CHUNK_SIZE);
	}
#if RASPPI >= 4
	else if (strcmp (pSoundDevice, "sndusb") == 0)
	{
		m_pSound = new CUSBSoundBaseDevice (SAMPLE_RATE);
	}
#endif
	else
	{
#ifdef USE_VCHIQ_SOUND
		m_pSound = new CVCHIQSoundBaseDevice (&m_VCHIQ, SAMPLE_RATE, CHUNK_SIZE,
					(TVCHIQSoundDestination) m_Options.GetSoundOption ());
#else
		m_pSound = new CPWMSoundBaseDevice (&m_Interrupt, SAMPLE_RATE, CHUNK_SIZE);
#endif
	}
	assert (m_pSound != 0);

	soundGenerator = new HyperNaturalSoundGenerator(*m_pSound, m_Logger, m_Scheduler, m_DeviceNameService, m_Serial, m_Timer);
	soundGenerator->samplesCheck();
	PrintMemoryInfo();
	soundGenerator->loop();

	m_Logger.Write (FromKernel, LogNotice, "FINALIZÓ LA EJECUCIÓN <3");

	// const char *mensaje = "--------- Hola desde Circle\r\n";
	// m_Serial.Write(mensaje, strlen(mensaje));
	// while (1) {
	// 	char c;
	// 	int nRead = m_Serial.Read(&c, 1);
	// 	if (nRead > 0) {
	// 		if (c == '1') {
	// 			m_ActLED.On();
	// 		}
	// 		else 
	// 			m_ActLED.Off();
	// 	}
	// }

	return ShutdownHalt;
}

void CKernel::PrintMemoryInfo()
{
    CMemorySystem *pMemory = CMemorySystem::Get();
    if (pMemory)
    {
        size_t totalMemory = pMemory->GetMemSize();  // Memoria total del sistema
        size_t freeHeapMemory = pMemory->GetHeapFreeSpace(HEAP_ANY);  // Memoria libre en el heap
        size_t usedMemory = totalMemory - freeHeapMemory;  // Memoria utilizada

        // Conversión a MB
        float totalMemoryMB = totalMemory / (1024.0 * 1024.0);
        float freeMemoryMB = freeHeapMemory / (1024.0 * 1024.0);
        float usedMemoryMB = usedMemory / (1024.0 * 1024.0);

        // Cálculo de porcentajes
        float freeMemoryPercentage = (totalMemory > 0) ? ((freeHeapMemory * 100.0) / totalMemory) : 0;
        float usedMemoryPercentage = 100.0 - freeMemoryPercentage;

        // Log de la información
        m_Logger.Write(FromKernel, LogNotice, "Memoria Total: %u bytes (%.2f MB)", totalMemory, totalMemoryMB);
        m_Logger.Write(FromKernel, LogNotice, "Memoria Usada: %u bytes (%.2f MB) - %.2f%%", usedMemory, usedMemoryMB, usedMemoryPercentage);
        m_Logger.Write(FromKernel, LogNotice, "Memoria Libre: %u bytes (%.2f MB) - %.2f%%", freeHeapMemory, freeMemoryMB, freeMemoryPercentage);
    }
    else
    {
        m_Logger.Write(FromKernel, LogError, "Error: No se pudo obtener la información de memoria");
    }
}