#include "SGSettings.h"

static enum State {
   WAIT_FOR_START,
   READ_NOTE,
   READ_NAME,
   READ_CHECKSUM
} state;

static uint8_t note = 0;
static char nameBuffer[MAX_SIZE_INSTRUMENT_NAME + 1];
static uint8_t nameIndex = 0;
static uint8_t expectedLength = 0;

SGSettings::SGSettings() {
   
}

bool SGSettings::readInfoInstruments() {

   // Enviando byte de preparando estado:
   // bool sgReady = false;
   // while (!sgReady) {
   //    Serial6.write(0xA);
   //    if (Serial6.available() > 0) {
   //       sgReady = true;
   //    }
   //    else {
   //       delay(500);
   //    }
   // }


   
   bool readedInfo = false;
   Serial.println("Entrando a la espera de HyperNatural Sound Generator");
   state = WAIT_FOR_START;
   // while (!readedInfo) {
      // leer los datos
      // Intentar leer todo el buffer

      // hacer un método de recuperación del protocolo más avanzado,

      // puede funcionar enviara datos redundantes, dos bytes de inicio, dos bytes de mensaje, etc.

      // implementar desde la raspberry un envío de los datos cada x tiempo y que ambos flujos se bloqueen

      // hasta que ambos flujos confirmen la información correcta. para que sea un protocolo robusto.

      // UPDATE: TODO: Por ahora la comunicación va a ser unidireccional del secuenciador hacia el módulo, esto para facilitar
      // la operación de comunicación, la limitante es que no se pueden aceptar nuevas configuraciones de sonidos dinámicamente
      // porque el secuenciador va a asumir que los instrumentos están en ciertos "posiciones" de memoria (índices quemados).

      digitalWrite(14, HIGH);

      delay(2000);

      while (Serial6.available()) {
         Serial.println(Serial6.read());
         delay(100);
      }

      digitalWrite(14, LOW);

      // while (Serial6.available()) {
      //    uint8_t byte = Serial6.read();
      //    switch (state) {
      //       case WAIT_FOR_START:
      //       if (byte == 0xF0) {
      //             Serial.println("Byte de inicio");
      //             state = READ_NOTE;
      //          }
      //          break;

      //       case READ_NOTE:
      //          note = byte;
      //          nameIndex = 0;
      //          memset(nameBuffer, 0, sizeof(nameBuffer));
      //          state = READ_NAME;
      //          break;

      //       case READ_NAME:
      //          nameBuffer[nameIndex++] = byte;
      //          if (nameIndex >= MAX_SIZE_INSTRUMENT_NAME) {
      //             // Protege de overflow
      //             nameIndex = MAX_SIZE_INSTRUMENT_NAME - 1;
      //          }
      //          state = READ_CHECKSUM;
      //          break;

      //       case READ_CHECKSUM:
      //          expectedLength = byte;
      //          nameBuffer[expectedLength] = '\0'; // Terminamos la cadena

      //          if (expectedLength == nameIndex) {
      //             // ✅ Mensaje válido
      //             Serial.println("Instrumento recibido:");
      //             Serial.print("Nota: "); Serial.println(note);
      //             Serial.print("Nombre: "); Serial.println(nameBuffer);
      //          } else {
      //             // ❌ Error: longitud no coincide
      //             Serial.println("Error: longitud de nombre no coincide");
      //          }

      //          state = WAIT_FOR_START;
      //          readedInfo = true;
      //          break;
      //    }
      // }



   // }
   return readedInfo;
}