#include <Arduino.h>

#define DEBUG_ETHERNET_WEBSERVER_PORT       Serial
#define _ETHERNET_WEBSERVER_LOGLEVEL_       3            // Debug Level from 0 to 4

#include <WebServer_WT32_ETH01.h>         // https://github.com/khoih-prog/WebServer_WT32_ETH01/
#include <AsyncTCP.h>                     // https://github.com/me-no-dev/AsyncTCP
#include <ESPAsyncWebServer.h>            // https://github.com/me-no-dev/ESPAsyncWebServer
#include <ElegantOTA.h>

#include <PicoMQTT.h>

PicoMQTT::Client mqtt("10.27.2.234");

// variaveis que indicam o núcleo
static uint8_t taskCoreZero = 0;
static uint8_t taskCoreOne = 1;

AsyncWebServer server(80);

// Set according to your local network if you need static IP
IPAddress myIP(10, 27, 2, 235);
IPAddress myGW(10, 27, 2, 1);
IPAddress mySN(255, 255, 252, 0);
IPAddress myDNS(8, 8, 8, 8);

WiFiClient    ethClient;

const int numPinos = 3;
const int numOutputPinos = 4;

const int outputPins[] = {4, 12, 14, 15};

String packSize = "--";
String packet ;

int pinsState[numPinos];
int pinsName[numPinos];

const int pinBotaoSilenciar = 35;
int lastStateBotaoSilenciar = false;

bool buzzerLigado = false;
bool buzzerSilenced = false;
int timer = 0;

unsigned long ota_progress_millis = 0;

void onOTAStart() {
  // Log when OTA has started
  Serial.println("OTA update started!");
  // <Add your own code here>
}

void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Serial.println("OTA update finished successfully!");
  } else {
    Serial.println("There was an error during OTA update!");
  }
  // <Add your own code here>
}

int timeroffline = 0;

void parsePacket(String packet){
  int i = 0;
  char *token;
  char *rest = (char *)packet.c_str();
  
  // Obtenha o primeiro token dividido por ponto-e-vírgula
  token = strtok_r(rest, ";", &rest);
  
  // Percorra os tokens divididos por ponto-e-vírgula
  while (token != NULL && i < numPinos) {
    // Divida o token atual em duas partes usando ":"
    char *name = strtok(token, ":");
    char *state = strtok(NULL, ":");
    
    if (name != NULL && state != NULL) {
      Serial.println("Parte 1: " + String(name) + ", Parte 2: " + String(state));

      pinsName[i] = atoi(name);
      pinsState[i] = atoi(state);

      i++;
    }
    
    // Obtenha o próximo token dividido por ponto-e-vírgula
    token = strtok_r(NULL, ";", &rest);
  }

}


void analizePinsState(){
  if(pinsState[0]){ // painel online
    digitalWrite(outputPins[0], HIGH);
  }else {
    digitalWrite(outputPins[0], LOW);
  }
  if(pinsState[1]){ // painel online
    digitalWrite(outputPins[1], HIGH);
  }else {
    digitalWrite(outputPins[1], LOW);
  }  
  if(pinsState[2]){ // painel online
    digitalWrite(outputPins[2], HIGH);
  }else {
    digitalWrite(outputPins[2], LOW);
  } 
  if((pinsState[1] || pinsState[2]) && !buzzerSilenced){
    digitalWrite(outputPins[3], HIGH);
    buzzerLigado = true;
  } else if((pinsState[1] || pinsState[2]) && buzzerSilenced) {
    digitalWrite(outputPins[3], LOW);
  } else {
    digitalWrite(outputPins[3], LOW);
    buzzerLigado = false;
  }
}

void loopTimer(void *pvParameters){
  while (true) {

    if(buzzerLigado && buzzerSilenced && timer < 60 * 5 /* 5 minutos*/){
      timer++;
    } else {
      timer = 0;
      buzzerSilenced = false;
    }

    if(timeroffline > 60 /*um minuto*/){
      digitalWrite(outputPins[0], HIGH);
    }
    timeroffline++;

    delay(1000); // delay de 1 segundo
  }
}
void loopButton(void *pvParameters){
  while (true) {
    if(digitalRead(pinBotaoSilenciar) == HIGH && !lastStateBotaoSilenciar){
      Serial.println("Botao ativo");
      lastStateBotaoSilenciar = true;
      buzzerSilenced = true;
      delay(500);
    }

    if(digitalRead(pinBotaoSilenciar) == LOW && lastStateBotaoSilenciar){
      lastStateBotaoSilenciar = false;
    }

    delay(500); // delay de 0.5 segundo
  }
}

void setup()
{
  for (int i = 0; i < numOutputPinos; i++) {
    pinMode(outputPins[i], OUTPUT);
    digitalWrite(outputPins[i], LOW);
  }

  pinMode(pinBotaoSilenciar, INPUT_PULLUP);

  pinMode(5, OUTPUT);
  Serial.begin(115200);

  while (!Serial);

  // Using this if Serial debugging is not necessary or not using Serial port
  //while (!Serial && (millis() < 3000));

  Serial.print("\nStarting MQTT_And_OTA_Ethernet on " + String(ARDUINO_BOARD));
  Serial.println(" with " + String(SHIELD_TYPE));
  Serial.println(WEBSERVER_WT32_ETH01_VERSION);

  // To be called before ETH.begin()
  WT32_ETH01_onEvent();

  ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER);

  // Static IP, leave without this line to get IP via DHCP
  ETH.config(myIP, myGW, mySN, myDNS);

  WT32_ETH01_waitForConnect();

  // Note - the default maximum packet size is 128 bytes. If the
  // combined length of clientId, username and password exceed this use the
  // following to increase the buffer size:
  // client.setBufferSize(255);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request)
  {
    request->send(200, "text/plain", "Hi! I am ESP32.");
  });

  ElegantOTA.begin(&server);    // Start ElegantOTA
  // ElegantOTA callbacks
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);

  server.begin();

  Serial.println();
  Serial.println("HTTP server started with MAC: " + ETH.macAddress() + ", at IPv4: " + ETH.localIP().toString());
  Serial.println();

  // Subscribe to a topic pattern and attach a callback
  mqtt.subscribe("bombas/deteccao", [](const char * topic, const char * payload) {
      Serial.printf("Received message in topic '%s': %s\n", topic, payload);
      parsePacket(String(payload));
      timeroffline=0;
  });


  // Start the broker
  mqtt.begin();

  Serial.println("broker started");

  Serial.println("start loops");
  xTaskCreatePinnedToCore(
      loopTimer,   /* função que implementa a tarefa */
      "loopTimer", /* nome da tarefa */
      10000,                 /* número de palavras a serem alocadas para uso com a pilha da tarefa */
      NULL,                  /* parâmetro de entrada para a tarefa (pode ser NULL) */
      0,                     /* prioridade da tarefa (0 a N) */
      NULL,                  /* referência para a tarefa (pode ser NULL) */
      taskCoreOne);         /* Núcleo que executará a tarefa */
  delay(2000); 
  Serial.println("start loops");
  xTaskCreatePinnedToCore(
      loopButton,   /* função que implementa a tarefa */
      "loopButton", /* nome da tarefa */
      10000,                 /* número de palavras a serem alocadas para uso com a pilha da tarefa */
      NULL,                  /* parâmetro de entrada para a tarefa (pode ser NULL) */
      0,                     /* prioridade da tarefa (0 a N) */
      NULL,                  /* referência para a tarefa (pode ser NULL) */
      taskCoreOne);         /* Núcleo que executará a tarefa */
  delay(2000);  
}

void loop()
{
  ElegantOTA.loop();
  mqtt.loop();

  analizePinsState();
}