/*
 * Version: 0.2
 * Changelog: 
 *    09/11/2017 17:05  * Adaptation pour la version 0.2 du programme digiMesh_sendSensorData.ino
 *                      * Enlever la variable mId  
 *    10/11/2017        1. Ajout un capteur de temperature d'eau                 
 */
#include <XBee.h>   // bibliotheque XBee

XBee xbee = XBee();   // Creer un objet xbee
XBeeResponse response = XBeeResponse(); 
// create reusable response objects for responses we expect to handle 
ZBRxResponse rx = ZBRxResponse();  // Creer un objet rx response

const int NBCAPTS = 4;
const int capteurPayloadLength = NBCAPTS * 4;
const int payloadStart = 11;

char nomTotem[3];

int statusLed = 13;   // definir le broche de LED de l'état

void flashLed(int pin, int times, int wait) {   // methode à faire cliqnoter LED defini
    for (int i = 0; i < times; i++) {
      digitalWrite(pin, HIGH);
      delay(wait);
      digitalWrite(pin, LOW);
      
      if (i + 1 < times) {
        delay(wait);
      }
    }
}

void setup() {
  // put your setup code here, to run once:
  pinMode(statusLed, OUTPUT);
  
  // start serial
  Serial.begin(9600);
  xbee.begin(Serial);
  
  flashLed(statusLed, 3, 50);     // faire clignoter 3 fois LED au demarrage
}

void loop() {
  // put your main code here, to run repeatedly:
  xbee.readPacket();                                      // lire les paquets entrants
  if (xbee.getResponse().getApiId() == ZB_RX_RESPONSE) {  // Afficher et extraire des trames entrants
      flashLed(statusLed, 5, 50);
      xbee.getResponse().getZBRxResponse(rx);   
      if (xbee.getResponse().isAvailable()) 
      {
          Serial.println(">--------------------------------------------------------------------------------<");
          int rfDataSize = xbee.getResponse().getFrameDataLength() - payloadStart; 
          int rfData[rfDataSize];
          for (int i = payloadStart, j = 0; i < (xbee.getResponse().getFrameDataLength()); i++, j++)
              rfData[j] = xbee.getResponse().getFrameData()[i];
    
    /////////////////////////////////////// Line 1: TOTEM ID ////////////////////////////////////////////////          
          int offset = 0;
          Serial.print("1) ");
          Serial.print("ID TOTEM: ");      
          for (int i = 0; i < 3; i++)
          {
            Serial.print((char)rfData[i]);
            nomTotem[i] = rfData[i];
            offset++;
          }
          Serial.println();

    /////////////////////////////////////// Line 2: Code Fonction ////////////////////////////////////////////////          
          Serial.print("2) ");
          Serial.print("Code fonction: ");      
          Serial.print(rfData[offset++], HEX);
          Serial.println();

    /////////////////////////////////////// Line 3: N° Paquet ////////////////////////////////////////////////          
          Serial.print("3) ");
          Serial.print("Numero paquet: ");      
          Serial.print((rfData[offset++])*256 + rfData[offset++]);
          Serial.println();     
    
    /////////////////////////////////////// Line 4: Données ////////////////////////////////////////////////          
          float donnees[4];
          uint8_t dataByte[4];
          Serial.print("4) ");
          Serial.println("Donnees: ");
          for (int i = 0; i < NBCAPTS; ++i)
          {
            for (int j = 0; j < 4; ++j)
            {
              dataByte[j] = rfData[offset++];  
            }
            donnees[i] = *((float*)&(dataByte));
          }
          Serial.print("\t Niveau (mm): ");
          Serial.println(donnees[0]);
          Serial.print("\t Temperature d'air (degree C): ");
          Serial.println(donnees[1]);
          Serial.print("\t Humidite (%): ");
          Serial.println(donnees[2]);
          Serial.print("\t Temperature d'eau (degree C): ");
          Serial.println(donnees[3]);
          Serial.println(">--------------------------------------------------------------------------------<");
      }
  }
}
