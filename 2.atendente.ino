#include <ESP8266WiFi.h> 
#include <WiFiClient.h>
#include <PubSubClient.h>

#define qtde165  1            //Registra o número de CIs 74HC165 cascateados
#define nExtIn qtde165 * 8    //Número de entradas do sistema
#define TempoDeslocamento 50  //Registra o tempo de queverá ter o pulso para leitura e gravação, (milissegundos)
#define press_time 250

// Declaração de variáveis globais
const int ploadPin        = 4;    //Conecta ao pino 1 do 74HC165 (LH/LD - asynchronous parallel load input)(PL) - D2 no ESP8266   D4 no ESP32
const int clockEnablePin  = 2;    //Conecta ao pino 15 do 74HC165 (CE - Clock Enable Input)(CE)                 - D4 no ESP8266   D2 no ESP32
const int dataPin         = 14;   //Conecta ao pino 9 do 74HC165 (Q7 - serial output from the last stage)(Q7)   - D5 no ESP8266   D14 no ESP32
const int clockPin        = 15;   //Conecta ao pino 2 do 74HC165 (CP - Clock Input)(CP)                         - D8 no ESP8266   D15 no ESP32
const int clockIn         = 13;   //Pino 11 Clock 74HC595                                                       - D7 no ESP8266   D13 no ESP32
const int latchPin        = 12;   //Pino 12 Latch 74HC595                                                       - D6 no ESP8266   D12 no ESP32
const int dataIn          = 0;    //Pino 14 Data  74HC595                                                       - D3 no ESP8266   D16 no ESP32
const int led_mqtt        = 5;    //LED que indica o status da conexão com o broker MQTT                        - D1 no ESP8266   D5 no ESP32
const int led_power       = 3;    //LED que indica o status da conexão com o banco de dados                     - RX no ESP8266   RX0 no ESP32
const int led_wifi        = 1;    //LED que indica a conexão com o wifi                                         - TX no ESP8266   TX0 no ESP32
const int led_db          = 17;   //LED que indica se o sistema está energizado                                 - -- no ESP8266   TX2 no ESP32

//const int ploadPin        = 5;    //Conecta ao pino 1 do 74HC165 (PL - asynchronous parallel load input)(SH/LD) - D2 no ESP8266
//const int clockEnablePin  = 2;    //Conecta ao pino 15 do 74HC165 (CE - Clock Enable Input)(CLK INH)            - D4 no ESP8266
//const int dataPin         = 4;    //Conecta ao pino 9 do 74HC165 (Q7 - serial output from the last stage)(QH)   - D5 no ESP8266
//const int clockPin        = 15;   //Conecta ao pino 2 do 74HC165 (CP - Clock Input)(CLK)                        - D8 no ESP8266
//const int clockIn         = 21;   //Pino 11 Clock 74HC595                                                       - D7 no ESP8266
//const int latchPin        = 17;   //Pino 12 Latch 74HC595                                                       - D6 no ESP8266
//const int dataIn          = 16;   //Pino 14 Data  74HC595                                                       - D0 no ESP8266
//const int led_mqtt        = 19;   //LED que indica o status da conexão com o broker MQTT                        - D1 no ESP8266
//const int led_wifi        = 23;   //LED que indica a conexão com o wifi                                         - RX no ESP8266
//const int led_power       = 22;   //LED que indica se o sistema está energizado                                 - D3 no ESP8266
//const int led_db          = 8;    //LED que indica o status da conexão com o banco de dados                     - -- no ESP8266

byte button_cmd[nExtIn] = {LOW};
byte LED_off[nExtIn] = {LOW};

unsigned long time_stp[nExtIn] = {0};

//WiFi
#define SECRET_SSID "FCA-Prod"
#define SECRET_PASS "FCAprod@TCA!"
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;        // your network password
int status = WL_IDLE_STATUS;      // status
WiFiClient wifiClient;                       
 
//MQTT Server
const char* BROKER_MQTT = "broker.hivemq.com";  //URL do broker MQTT que se deseja utilizar
int BROKER_PORT = 1883;                         // Porta do Broker MQTT

#define ID_MQTT  "STL02"             //Informe um ID unico e seu. Caso sejam usados IDs repetidos a ultima conexão irá sobrepor a anterior. 
#define TOPIC_SUBSCRIBE "STLclient1"   //Informe um Tópico único. Caso sejam usados tópicos em duplicidade, o último irá eliminar o anterior.
#define TOPIC_PUBLISH "STLclient2"

PubSubClient MQTT(wifiClient);        // Instancia o Cliente MQTT passando o objeto espClient

//Declaração das Funções
void mantemConexoes();  //Garante que as conexoes com WiFi e MQTT Broker se mantenham ativas
void conectaWiFi();     //Faz conexão com WiFi
void conectaMQTT();     //Faz conexão com Broker MQTT
void recebePacote(char* topic, byte* payload, unsigned int length);
void apagaLEDs();

void setup() {
  pinMode(dataIn, OUTPUT);
  pinMode(clockIn, OUTPUT);
  pinMode(latchPin, OUTPUT);
  pinMode(ploadPin, OUTPUT);
  pinMode(clockEnablePin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, INPUT);
  pinMode(led_mqtt, OUTPUT);
  pinMode(led_wifi, OUTPUT);
  pinMode(led_power, OUTPUT);
    
  digitalWrite(led_mqtt, LOW);
  digitalWrite(led_wifi, LOW);
  digitalWrite(led_power, HIGH);

  digitalWrite(clockPin, HIGH);
  digitalWrite(ploadPin, HIGH); 

  for(int i = 0; i < nExtIn; i++){
    time_stp[i] = millis();
  }

  Serial.begin(9600);

  conectaWiFi();
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);  
  MQTT.setCallback(recebePacote);  
}

void loop() {
  mantemConexoes();
  read_shift_regs();
  send_to_LEDs();
  MQTT.loop();
}

void mantemConexoes() {
    if (!MQTT.connected()) {
       conectaMQTT(); 
    }
    
    conectaWiFi(); //se não há conexão com o WiFI, a conexão é refeita
}

void conectaWiFi() {

  if (WiFi.status() == WL_CONNECTED) {
     return;
  }
        
  Serial.print("Conectando-se na rede: ");
  Serial.print(ssid);
  Serial.println("  Aguarde!");

  WiFi.begin(ssid, pass); // Conecta na rede WI-FI  
  while (WiFi.status() != WL_CONNECTED) {
      digitalWrite(led_wifi, LOW);
      delay(100);
      Serial.print(".");
  }
  digitalWrite(led_wifi, HIGH);
  Serial.println();
  Serial.print("Conectado com sucesso, na rede: ");
  Serial.print(ssid  );  
  Serial.print("  IP obtido: ");
  Serial.println(WiFi.localIP()); 
}

void conectaMQTT() { 
    while (!MQTT.connected()) {
        Serial.print("Conectando ao Broker MQTT: ");
        Serial.println(BROKER_MQTT);
        digitalWrite(led_mqtt, LOW);
        if (MQTT.connect(ID_MQTT)) {
            Serial.println("Conectado ao Broker com sucesso!");
            MQTT.subscribe(TOPIC_SUBSCRIBE);
            digitalWrite(led_mqtt, HIGH);
        } 
        else {
            Serial.println("Nao foi possivel se conectar ao broker.");
            Serial.println("Nova tentativa de conexao em 10s");
            delay(10000);
        }
    }
}

//Função para definir um rotina shift-in, lê os dados do 74HC165
void read_shift_regs()
{
    int bitVal=0;
    
    digitalWrite(clockEnablePin, HIGH);
    digitalWrite(ploadPin, LOW);
    delayMicroseconds(TempoDeslocamento);
    digitalWrite(ploadPin, HIGH);
    digitalWrite(clockEnablePin, LOW);

    // Efetua a leitura de um bit da saida serial do 74HC165
    for(int i = 0; i < nExtIn; i++)
    {
        bitVal = digitalRead(dataPin);

        LED_off[i] = comandoLED(bitVal, LED_off[i],i);

        //Lança um pulso de clock e desloca o próximo bit
        digitalWrite(clockPin, HIGH);
        delayMicroseconds(TempoDeslocamento);
        digitalWrite(clockPin, LOW);

        if (button_cmd[i] && LED_off[i]){
          int x = nExtIn - i;
          Serial.print("Apagar LED ");
          Serial.println(x);
          button_cmd[i] = 0;
          apagaLEDs();          
        }
    }
}

int comandoLED(int pinValues, int command, int i)
{
  unsigned long x = millis() - time_stp[i];

  if(pinValues == 1){
    if (x < 5000){
      return command;
    }
    else{
      command = 1;
      time_stp[i] = millis();
      delay(press_time);
      Serial.print("Comando para apagar LED ");
      Serial.println(i);
      Serial.println(pinValues);
    }
  }

  else if(pinValues == 0){
    command = 0;
  }
  return (byte)command;
}

//Envia informações aos LEDs
void send_to_LEDs()
{
  digitalWrite(latchPin, LOW);      //começa a enviar dados

  for(int i = 0; i < nExtIn; i++)
  {
    digitalWrite(clockIn, LOW);
    digitalWrite(dataIn, button_cmd[i]);
    delayMicroseconds(TempoDeslocamento);
    digitalWrite(clockIn, HIGH);
    digitalWrite(dataIn, LOW);
  }
  digitalWrite(latchPin, HIGH);
}

void recebePacote(char* topic, byte* payload, unsigned int length) 
{
  //obtem a leitura do payload recebido
  for(int i = 0; i < length; i++){
    button_cmd[i] = payload[i];
    Serial.print(button_cmd[i]);
  }
  Serial.println(" Pacote recebido");
}

void apagaLEDs(){
  boolean sent = MQTT.publish(TOPIC_PUBLISH, LED_off, nExtIn, false);
  if (sent == false){
    Serial.println("Erro em envio de Payload.");
  } 
  else{
    Serial.println("Payload enviado.");
  }
}
