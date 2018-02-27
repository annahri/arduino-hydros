// Envoi des données de mesure par XBee //
/* Appels des bibliotheques */
#include <DHT.h>      // Bibliothque pour le capteur d'humidite et de temperature d'air
#include <DallasTemperature.h>// Bibliotheque pour le capteur de temperateur d'eau
#include <OneWire.h>  // Bibliotheque pour capteur de temperateur
#include <XBee.h>     // Bibliotheque pour XBee
#include <Timer.h>     // Bibliotheque pour le planificateur 
#include <SD.h>

/* Definir des valeurs constantes */
// HC-SR04 (capteur d'humidite et de temperature d'air) //
#define TRIGG 46     // Broche TRIGGER, 52 d'arduino
#define ECHO 47       // Broche ECHPO, 53 d'arduino

// DHT22 (capteur d'ultrason) //
#define DHTPIN 48     // On selectionne la broche où nous allons connecter le capteur
#define DHTTYPE DHT22 // On selectionne le capteur DHT22 (Il y a d'autres types de capteur DHT)

// DS18D20 (capteur de temperature d'eau) //
#define PIN 50        // On utilise la broche 50 où se branche le data
OneWire ourWire(PIN); // On prend la broche comme bus pour la communication OneWire
DallasTemperature sensors(&ourWire); // On appelle la bibliotheque DallasTemperature
DHT dht(DHTPIN, DHTTYPE); // On initialise les variables qui seront utilisées pour la communication

// Module carte SD
#define SD_CS 4 
File donnees_fichier;

// On crée un objet xbee
XBee xbee = XBee();   

// on definit le Timeout
const long TIMEOUT = 25000UL; // 25ms = ~8m a  340m/s

// planificateur
Timer timer;

// on definit un tableau pour stocker les données de mesure et à envoyer
// on a 4 fois 4 octets
// la date et le temps n'a pas encore pris en compte
const int NBCAPTS = 4;
const int NOMS = 3;
// Nombre de releve
int NB = 10;
char idTotem[] = "T01"; // Changer le nom du Totem ici
const int tailleA = NOMS + 3 + (NBCAPTS * 4); // defaut: 22 octets
const int tailleB = NOMS + 1 + 11 + 2 + 2 + 30; // defaut: 53 octets
uint8_t payloadCapt[tailleA]; 
uint8_t payloadImg[tailleB];
// Numero du paquet
int count;
// Pour couper une valeur flottant en 4 octets
union u_tag 
{
  uint8_t b[4];
  float fval;
} u;
// Payload Image
int numero;
// Adresse Totem
const uint32_t adresseSH = 0x0013a200;
const uint32_t adresseSL = 0x40d4b1eb;


// Adressage de XBee récépteur (maitre)
XBeeAddress64 addr64 = XBeeAddress64(adresseSH, adresseSL); // SH + SL de Totem maitre
ZBTxRequest dgTx = ZBTxRequest(addr64, payloadCapt, sizeof(payloadCapt)); 
ZBTxRequest dgTxImg = ZBTxRequest(addr64, payloadImg, sizeof(payloadImg)); 
ZBTxStatusResponse txStatus = ZBTxStatusResponse();

// On definit LED 
int statusLed = 13;
int errorLed = 13;

// Methode pour faire clignoter LED defini
void flashLed(int pin, int times, int wait) {
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
  pinMode(TRIGG, OUTPUT);  //Configuration des broches
  digitalWrite(TRIGG, LOW); // La broche TRIGGER doit etre a  LOW au repos
  pinMode(ECHO, INPUT);
  pinMode(statusLed, OUTPUT);
  pinMode(errorLed, OUTPUT);
  
  // remplir l'ID totem à l'indice 0 - 2 dans payloads
  for (int i = 0; i < NOMS; i++) {
    payloadCapt[i] = idTotem[i];
    payloadImg[i] = idTotem[i];
  }
  
  Serial.begin(9600);     // Demarrage de la liaison serie
  xbee.setSerial(Serial); // Demarrage de XBee
  sensors.begin();        // On initialise les capteurs
  dht.begin();            // On initialise le capteur
  delay(1000);
  
  // Si on commence la librairie SD et lui ne detecte pas la SD ou il se trouve dans une broche different
  // On entre dans une boucle pour le verifier et chercher une solution, sinon la connexion est bonne
  int c = 0;
  while (!SD.begin(SD_CS)) {
    flashLed(errorLed, 3, 500);
    delay(1000);
    c++;
    if (c == 5) break;
  }
  flashLed(errorLed, 5, 100);


  // On envoi 3 fois les données par jour, donc 24 / 3 = 8
  // 8 * 60 * 60 * 1000 traduit 8 heures sous forme ms
  // cette fonction permet d'appeler le methode "EnvoyerDonneesCapts" 
  // chaque temps defini sans arreter l'arduino (en utilisant 'delay')
  int envDonnee = timer.every(2*1000, EnvoyerDonneesCapts);
  // Appeler la methode tous les 12 heures ou 2 fois par jour
  //int envPhoto = timer.every(12*60*60*1000, AffecterPayloadImg);

  //EnvoyerPayloadImg();

}

void loop() {
  timer.update();
  //EnvoyerDonneesCapts(10, 2000);
}

void EnvoyerDonneesCapts() {
  /* Dans ce programme, les données acquisent depuis les capteurs sont mises en ordre :
  *   L'ordre des capteurs
  *   1. Capteur du niveau
  *   2. Capteur de temperature d'eau
  *   3. Capteur d'humidité
  *   4. Capteur de temperature d'air
  **/
  bool dataSent = false;
  float somme[NBCAPTS];
  float mesures[NBCAPTS];
  for (int i = 0; i < NBCAPTS; i++) { somme[i]=0; }
  
  /* ===== capteur d'ultrasons ===== */
    for (int x = NB; x > 0; x--) {
      digitalWrite(TRIGG, HIGH); // Lance une mesure de distance en envoyant
      delayMicroseconds(10);     // une impulsion HIGH de 10 µs sur la broche TRIGGER
      digitalWrite(TRIGG, LOW);
      float son = 340.0 / 1000;  // vitesse du son dans l'air (mm/Âµs)
      int mesure = pulseIn(ECHO, HIGH, TIMEOUT); // Mesure le temps entre
      // l'envoi d e l'ultrason et sa reception
      float distance_mm = mesure / 2.0 * son;    // calcul de la distance grace au temps
      //on divise par 2 car le son fait un aller-retour
      somme[0] += distance_mm;
    }
  /* ============================== */
    
  /* ===== capteur de temperature d'eau ===== */
    for(int x = NB; x > 0; x--)
    {
      sensors.requestTemperatures();       //On initialise le capteur pour la mesure
      somme[1] += sensors.getTempCByIndex(0);
    }
  /* ============================== */
  
  /* ===== capteur d'humidite + temperature d'air ===== */
    for(int x = NB; x > 0; x--)
    {
      float h = dht.readHumidity(); //Acquisition de la mesure d'humidité
      float t = dht.readTemperature(); //Acquisition de la mesure de température
      somme[2] += h;
      somme[3] += t;  
    }
  /* ============================== */
  
  /* ===== Calcul de la moyenne ===== */
    float moy[NBCAPTS];
    for (int i = 0; i < NBCAPTS; i++) {
      moy[i] = somme[i] / NB;
    }
  
  // Tous les variables (distance_mm, h, t) sont remplis 
  
  /* Inserer la donnée de mesure dans le payload */
  // convertir la donnée en un tableau d'octets et le copier dans le tableau du payload
  // voir le documentation pour le protocol d'application
    // 0 = trame d'envoi des données de mesure
    payloadCapt[3] = 0;     
    // numero de la trame, msb lsb 
    payloadCapt[4] = (byte) (count >> 8); 
    payloadCapt[5] = (byte) (count & 0xFF); 
    // daStart = data start. C'est la premiere indice où se trouve les données. 
    // la premiere boucle va s'arreter selon le nombre des capteurs
    // ici, daStart commence à l'indice 6   
    int daStart = NOMS+3;
      for (int i = 0; i < NBCAPTS; i++) {
      // x a pour but de balayer dans 'u'
      int x = 0;
      // u prend la donnée flottant et la stocker en 4 octets
      u.fval = moy[i];
      // la deuxieme boucle va balayer depuis daStart jusqua daStart + 4, 
      // parce que chaque capteur possede 4 octets
      // j commence par daStart qui est initialement 6, et s
      for(int j = daStart; j < daStart+4; j++)
      {
        // payLoadCapt[j] va prendre un par un les 4 octets de la donnée flottant.
        // on veut que u.b[x] va de 0 à 3, à chaque interation de cet boucle
        payloadCapt[j] = u.b[x];
        x++;
      }
      daStart += 4;
    }
  
  TransmettrePayload(dgTx, 0);
}

void PrendrePhoto() {
  // les codes pour prendre une photo

}

char * EntetePayloadImg() {
  char * nomfichier = "image00.jpg";

  // Code fonction 1 : Payload d'image
  payloadImg[3] = 1;
  for (int i = 0; i < 11; ++i) payloadImg[i+4] = nomfichier[i];

  int nombre = 0;
  donnees_fichier = SD.open(nomfichier);
  if (donnees_fichier) {
    while(donnees_fichier.available()) {
      for (int i = 0; i < 30; ++i) donnees_fichier.read();
      nombre++;
    }
    donnees_fichier.close();
  }
  Serial.print(nombre);
  // Nombre de paquets à envoyer
  payloadImg[17] = (byte) (nombre >> 8);
  payloadImg[18] = (byte) (nombre & 0xFF);

  return nomfichier;
}

void NumeroPayloadImage() {  
  // Numero du paquet
  payloadImg[15] = (byte) (numero >> 8) ;
  payloadImg[16] = (byte) (numero & 0xFF);
}

void EnvoyerPayloadImg() {  
  numero = 0;
  char * nomfichier = EntetePayloadImg();
  donnees_fichier = SD.open(nomfichier);
  if (donnees_fichier) {
    byte dataLine;
    // index = l'indice ou se trouve les données d'image
    while(donnees_fichier.available()) {
      int index = 19;
      for (int i = 0; i < 30; ++i) {
        dataLine = donnees_fichier.read();
        payloadImg[index++] = dataLine;
      }
      NumeroPayloadImage();
      TransmettrePayload(dgTxImg, 1);
    }
    donnees_fichier.close();
  }
}

void TransmettrePayload(ZBTxRequest trame, int type) {
  /* ============================== */
  // Cette boucle permet d'envoyer une fois le paquet quand le totem maitre le recoit
  // et refaire l'envoi quand le paquet est perdu
  if (count > 65534) { count = 0; }
  bool dataSent = false;
  while(!dataSent) {
    xbee.send(trame);
    delay(2000);
    if (xbee.readPacket(500)) {
      if (xbee.getResponse().getApiId() == ZB_TX_STATUS_RESPONSE) {
          xbee.getResponse().getZBTxStatusResponse(txStatus);
          if (txStatus.getDeliveryStatus() == SUCCESS) {
            // la trame est delivré
            // mise en vrai le dataSent, puis incrementer le count selon le type du payload  
            dataSent = true;
            flashLed(statusLed, 5, 50);
            if (type == 0) count++;
            else if (type == 1) numero++;
          } else flashLed(errorLed, 3, 500);
      } else flashLed(errorLed, 2, 500);
      delay(2000/2);
    }
  }
}