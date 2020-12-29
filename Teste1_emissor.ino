#include <WiFi.h> 
#include <PubSubClient.h>

// Definições de Labels
#define qtde165  1            //Registra o número de CIs 74HC165 cascateados
#define nExtIn qtde165 * 8
#define TempoDeslocamento 15  //Registra o tempo de queverá ter o pulso para leitura e gravação, (milesegundos)
#define press_time 250

// Declaração de variáveis globais
const int ploadPin        = 4;    //Conecta ao pino 1 do 74HC165 (LH/LD - asynchronous parallel load input)(PL)
const int clockEnablePin  = 2;    //Conecta ao pino 15 do 74HC165 (CE - Clock Enable Input)(CE)
const int dataPin         = 14;   //Conecta ao pino 9 do 74HC165 (Q7 - serial output from the last stage)(Q7)
const int clockPin        = 15;   //Conecta ao pino 2 do 74HC165 (CP - Clock Input)(CP)

byte button_cmd[nExtIn] = {LOW};
byte button_cmd_old[nExtIn] = {LOW};

unsigned long time_stp[nExtIn] = {0};

//WiFi
const char* SSID = "...";                // SSID / nome da rede WiFi que deseja se conectar
const char* PASSWORD = "qualidade";   // Senha da rede WiFi que deseja se conectar
WiFiClient wifiClient;                        
 
//MQTT Server
const char* BROKER_MQTT = "broker.hivemq.com"; //URL do broker MQTT que se deseja utilizar
int BROKER_PORT = 1883;                      // Porta do Broker MQTT

#define ID_MQTT  "FCA01"            //Informe um ID unico e seu. Caso sejam usados IDs repetidos a ultima conexão irá sobrepor a anterior. 
#define TOPIC_PUBLISH "FCABotao1"    //Informe um Tópico único. Caso sejam usados tópicos em duplicidade, o último irá eliminar o anterior.
PubSubClient MQTT(wifiClient);        // Instancia o Cliente MQTT passando o objeto espClient

//Declaração das Funções
void mantemConexoes();  //Garante que as conexoes com WiFi e MQTT Broker se mantenham ativas
void conectaWiFi();     //Faz conexão com WiFi
void conectaMQTT();     //Faz conexão com Broker MQTT
void enviaValores();     //

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

        button_cmd[i] = on_off_status(bitVal, button_cmd[i], i);

        if(button_cmd[i] != button_cmd_old[i]){
          boolean sent = MQTT.publish(TOPIC_PUBLISH, button_cmd, nExtIn, true);
          if (sent == true){
            Serial.println("Payload enviado.");
            button_cmd_old[i] == button_cmd[i];
          }
          else {
            Serial.println("Erro em envio de Payload.");
          }
        }

        //Lança um pulso de clock e desloca o próximo bit
        digitalWrite(clockPin, HIGH);
        delayMicroseconds(TempoDeslocamento);
        digitalWrite(clockPin, LOW);
    }
}

//Verifica estado dos botões
int on_off_status (int pinValues, int command, int i)
{
  
  unsigned long x = millis() - time_stp[i];
  
  if(pinValues == 1){
    if (x < 5000){
      return command;
    }
    else{
      command = !command;
      time_stp[i] = millis();
      delay(press_time);
    }
  }
  return (byte)command;
}

void setup() {
   //Inicializa e configura os pinos
    pinMode(ploadPin, OUTPUT);
    pinMode(clockEnablePin, OUTPUT);
    pinMode(clockPin, OUTPUT);
    pinMode(dataPin, INPUT);

    digitalWrite(clockPin, HIGH);
    digitalWrite(ploadPin, HIGH);        

  Serial.begin(115200);

  conectaWiFi();
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);   
}

void loop() {
  mantemConexoes();
  MQTT.loop();

    //Lê todos as portas externas
    read_shift_regs(); 
    
    //Envia comando para a rede
    //enviaValores();
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
        } 
        else {
            Serial.println("Noo foi possivel se conectar ao broker.");
            Serial.println("Nova tentativa de conexao em 10s");
            delay(10000);
        }
    }
}

/*void enviaValores(){
  for (int i = 0; i < nExtIn; i++){
    if(button_cmd[i] != button_cmd_old[i]){
      boolean sent = MQTT.publish(TOPIC_PUBLISH, button_cmd, nExtIn, true);
      if (sent == true){
        Serial.println("Payload enviado.");
        button_cmd_old[i] == button_cmd[i];
      }
      else {
        Serial.println("Erro em envio de Payload.");
      }
    }
  }
} */
