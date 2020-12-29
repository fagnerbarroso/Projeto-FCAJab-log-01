#include <WiFi.h> 
#include <PubSubClient.h>

#define qtde165  1            //Registra o número de CIs 74HC165 cascateados
#define nExtIn qtde165 * 8
#define TempoDeslocamento 50  //Registra o tempo de queverá ter o pulso para leitura e gravação, (milissegundos)

// Declaração de variáveis globais
const int clockIn         = 4;    //Pino 11 Clock 74HC595
const int latchPin        = 2;    //Pino 12 Latch 74HC595
const int dataIn          = 14;    //Pino 14 Data  74HC595

int button_cmd[nExtIn] = {LOW};

//WiFi
const char* SSID = "...";                // SSID / nome da rede WiFi que deseja se conectar
const char* PASSWORD = "qualidade";   // Senha da rede WiFi que deseja se conectar
WiFiClient wifiClient;                        
 
//MQTT Server
const char* BROKER_MQTT = "broker.hivemq.com"; //URL do broker MQTT que se deseja utilizar
int BROKER_PORT = 1883;                      // Porta do Broker MQTT

#define ID_MQTT  "FCA02"             //Informe um ID unico e seu. Caso sejam usados IDs repetidos a ultima conexão irá sobrepor a anterior. 
#define TOPIC_SUBSCRIBE "FCABotao1"   //Informe um Tópico único. Caso sejam usados tópicos em duplicidade, o último irá eliminar o anterior.
PubSubClient MQTT(wifiClient);        // Instancia o Cliente MQTT passando o objeto espClient

//Declaração das Funções
void mantemConexoes();  //Garante que as conexoes com WiFi e MQTT Broker se mantenham ativas
void conectaWiFi();     //Faz conexão com WiFi
void conectaMQTT();     //Faz conexão com Broker MQTT
void recebePacote(char* topic, byte* payload, unsigned int length);

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

void setup() {
    pinMode(dataIn, OUTPUT);
    pinMode(clockIn, OUTPUT);
    pinMode(latchPin, OUTPUT);

  Serial.begin(115200);

  conectaWiFi();
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);  
  MQTT.setCallback(recebePacote); 
}

void loop() {
  mantemConexoes();
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
  Serial.print(SSID);
  Serial.println("  Aguarde!");

  WiFi.begin(SSID, PASSWORD); // Conecta na rede WI-FI  
  while (WiFi.status() != WL_CONNECTED) {
      delay(100);
      Serial.print(".");
  }
  
  Serial.println();
  Serial.print("Conectado com sucesso, na rede: ");
  Serial.print(SSID);  
  Serial.print("  IP obtido: ");
  Serial.println(WiFi.localIP()); 
}

void conectaMQTT() { 
    while (!MQTT.connected()) {
        Serial.print("Conectando ao Broker MQTT: ");
        Serial.println(BROKER_MQTT);
        if (MQTT.connect(ID_MQTT)) {
            Serial.println("Conectado ao Broker com sucesso!");
            MQTT.subscribe(TOPIC_SUBSCRIBE);
        } 
        else {
            Serial.println("Noo foi possivel se conectar ao broker.");
            Serial.println("Nova tentatica de conexao em 10s");
            delay(10000);
        }
    }
}

void recebePacote(char* topic, byte* payload, unsigned int length) 
{
    //obtem a string do payload recebido
    for(int i = 0; i < length; i++) 
    {
       button_cmd[i] = (int)payload[i];
    }

    send_to_LEDs();
}
