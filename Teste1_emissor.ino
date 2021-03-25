//Programado para usar no ESP32

// INCLUSÃO DE BIBLIOTECAS
#include <WiFi.h>
#include <WiFiClient.h>
#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>
#include <PubSubClient.h>

#include "ESP32_secrets.h"

// Definições de Labels
#define qtde165  1            //Registra o número de CIs 74HC165 cascateados
#define nExtIn qtde165 * 8
#define TempoDeslocamento 50  //Registra o tempo de queverá ter o pulso para leitura e gravação, (milesegundos)
#define press_time 250

// Declaração de variáveis globais
const int ploadPin        = 4;    //Conecta ao pino 1 do 74HC165 (LH/LD - asynchronous parallel load input)(PL) - D2 no ESP8266
const int clockEnablePin  = 2;    //Conecta ao pino 15 do 74HC165 (CE - Clock Enable Input)(CE)                 - D4 no ESP8266
const int dataPin         = 14;   //Conecta ao pino 9 do 74HC165 (Q7 - serial output from the last stage)(Q7)   - D5 no ESP8266
const int clockPin        = 15;   //Conecta ao pino 2 do 74HC165 (CP - Clock Input)(CP)                         - D8 no ESP8266
const int clockIn         = 13;   //Pino 11 Clock 74HC595                                                       - D7 no ESP8266
const int latchPin        = 12;   //Pino 12 Latch 74HC595                                                       - D6 no ESP8266
const int dataIn          = 16;   //Pino 14 Data  74HC595                                                       - D0 no ESP8266
const int led_mqtt        = 5;    //LED que indica o status da conexão com o broker MQTT                        - D1 no ESP8266  
const int led_db          = 3;    //LED que indica o status da conexão com o banco de dados                     - 
const int led_wifi        = 1;    //LED que indica a conexão com o wifi                                         - 
const int led_power       = 17;   //LED que indica se o sistema está energizado                                 - 
 
byte button_cmd[nExtIn] = {LOW};
byte button_cmd_old[nExtIn] = {LOW};
byte LED_off[nExtIn] = {LOW};
char *last_call_client[nExtIn];

unsigned long time_stp[nExtIn] = {0};

//WiFi
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;        // your network password
int status = WL_IDLE_STATUS;      // status
WiFiClient wifiClient;   

// INSTANCIANDO OBJETOS
WiFiClient client;
MySQL_Connection conn((Client *)&client);
 
//MQTT Server
const char* BROKER_MQTT = "broker.hivemq.com";  //URL do broker MQTT que se deseja utilizar
int BROKER_PORT = 1883;                         // Porta do Broker MQTT

#define ID_MQTT  "STL01"             //Informe um ID unico e seu. Caso sejam usados IDs repetidos a ultima conexão irá sobrepor a anterior. 
#define TOPIC_PUBLISH "STLclient1"    //Informe um Tópico único. Caso sejam usados tópicos em duplicidade, o último irá eliminar o anterior.
#define TOPIC_SUBSCRIBE "STLclient2"

PubSubClient MQTT(wifiClient);        // Instancia o Cliente MQTT passando o objeto espClient

// DECLARAÇÃO DE VARIÁVEIS PARA MySQL
IPAddress server_addr(85, 10, 205, 173);  // IP of the MySQL *server* here
char user[] = SECRET_USERDB;              // MySQL user login username
char password[] = SECRET_PASSDB;          // MySQL user login password

//char INSERT_SQL[] = "INSERT INTO fagnerdb.TesteTab1 (Status, Horario) VALUES ('%s', current_timestamp)";
//char query[128];

//Declaração das Funções
void mantemConexoes();    //Garante que as conexoes com WiFi e MQTT Broker se mantenham ativas
void conectaWiFi();       //Faz conexão com WiFi
void conectaBD();         //Conecta no Banco de Dados
void conectaMQTT();       //Faz conexão com Broker MQTT
void recebeLEDoff(char* topic, byte* payload, unsigned int length);
void enviaValoresMQTT();  //Envia valores para o cliente subscriber
void enviaBD();


void setup() {
    pinMode(ploadPin, OUTPUT);
    pinMode(clockEnablePin, OUTPUT);
    pinMode(clockPin, OUTPUT);
    pinMode(dataPin, INPUT);
    pinMode(dataIn, OUTPUT);
    pinMode(clockIn, OUTPUT);
    pinMode(latchPin, OUTPUT);
    pinMode(led_mqtt, OUTPUT);
    pinMode(led_db, OUTPUT);
    pinMode(led_wifi, OUTPUT);
    pinMode(led_power, OUTPUT);
    
    digitalWrite(led_mqtt, LOW);
    digitalWrite(led_db, LOW);
    digitalWrite(led_wifi, LOW);
    digitalWrite(led_power, HIGH);

    digitalWrite(clockPin, HIGH);
    digitalWrite(ploadPin, HIGH); 

    for(int i = 0; i < nExtIn; i++){
      time_stp[i] = millis();
    }

  Serial.begin(115200);

  conectaWiFi();
  MQTT.setServer(BROKER_MQTT, BROKER_PORT); 
  MQTT.setCallback(recebeLEDoff);  
  conectaBD();
}

void loop() {
  mantemConexoes();
  
  read_shift_regs();

  //Envia comando para os LEDs
  send_to_LEDs();
  
  MQTT.loop();
}

void mantemConexoes() {
    if (!MQTT.connected()) {
       conectaMQTT(); 
    }
    
    conectaWiFi();        //se não há conexão com o WiFI, a conexão é refeita
}

void conectaWiFi() {

  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(led_wifi, HIGH);
     return;
  }
        
  digitalWrite(led_wifi, LOW);
  Serial.print("Conectando-se na rede: ");
  Serial.print(ssid);
  Serial.println("  Aguarde!");

  WiFi.begin(ssid, pass); // Conecta na rede WI-FI  
  while (WiFi.status() != WL_CONNECTED) {
      delay(100);
      Serial.print(".");
  }
  
  Serial.println();
  Serial.print("Conectado com sucesso, na rede: ");
  Serial.print(ssid);  
  Serial.print("  IP obtido: ");
  Serial.println(WiFi.localIP()); 
}

void conectaMQTT() { 
    while (!MQTT.connected()) {
        Serial.print("Conectando ao Broker MQTT: ");
        Serial.println(BROKER_MQTT);
        if (MQTT.connect(ID_MQTT)) {
            digitalWrite(led_mqtt, HIGH);
            Serial.println("Conectado ao Broker com sucesso!");
            MQTT.subscribe(TOPIC_SUBSCRIBE);
        } 
        else {
            digitalWrite(led_mqtt, LOW);
            Serial.println("Nao foi possivel se conectar ao broker.");
            Serial.println("Nova tentativa de conexao em 10s");
            delay(10000);
        }
    }
}

void enviaValoresMQTT() {
  boolean sent = MQTT.publish(TOPIC_PUBLISH, button_cmd, nExtIn, true);
  if (sent == false){
    Serial.println("Erro em envio de Payload.");
  }
  else{
    for(int i = 0; i < nExtIn; i++){
      Serial.print(button_cmd[i]);
    }
    Serial.println(" Payload enviado");
  }
}      

//Função para definir um rotina shift-in, lê os dados do 74HC165
void read_shift_regs()
{
    int bitVal = 0;
    
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
        
        //Lança um pulso de clock e desloca o próximo bit
        digitalWrite(clockPin, HIGH);
        delayMicroseconds(TempoDeslocamento);
        digitalWrite(clockPin, LOW);

        if(!button_cmd_old[i] && button_cmd[i]){
          int x = nExtIn - i;
          Serial.print("Acender LED ");
          Serial.println(x);
          last_call_client[i] = "Local";
          enviaValoresMQTT();
          enviaBD(x, (int)button_cmd[i], 0);
          Serial.println((int)button_cmd[i]);
        }

        else if ((button_cmd_old[i] && !button_cmd[i]) && (last_call_client[i] != "Remoto")){
          int x = nExtIn - i;          
          Serial.print("Apagar LED ");
          Serial.println(x);
          last_call_client[i] = "Local";
          enviaValoresMQTT();
          enviaBD(x, (int)button_cmd[i], 0);
          Serial.println((int)button_cmd[i]);
        }
        button_cmd_old[i] = button_cmd[i];
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

void recebeLEDoff(char* topic, byte* payload, unsigned int length) 
{
  //obtem a leitura do payload recebido
  for(int i = 0; i < length; i++){
    LED_off[i] = payload[i];
    apaga_LEDs(LED_off[i], i);
  }
}

void apaga_LEDs(int LED_off, int i){
  if(LED_off == 1){
    button_cmd[i] = 0;
    time_stp[i] = millis();
    int x = nExtIn - i;
    last_call_client[i] = "Remoto";
    enviaBD(x, (int)button_cmd[i], 1);
    Serial.print("Comando cliente kanban. Apagar LED ");
    Serial.println(x);
  }
}

void conectaBD(){
  // CONECTA NO MySQL
  while (!conn.connect(server_addr, 3306, user, password)) {
    digitalWrite(led_db, LOW);
    Serial.println("Conexão SQL falhou.");
    conn.close();
    delay(1000);
    Serial.println("Conectando SQL novamente.");
  }
  Serial.println("Conectado ao servidor SQL.");
  digitalWrite(led_db, HIGH);
}

void enviaBD(int i, int local, int outro){
  if(local == 0 && outro == 0){
    char INSERT_SQL[] = "INSERT INTO fagnerdb.jablog  SELECT bl.LOCATION, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP,  ml.MATERIAL, 'Local', 'Cancelado'  FROM fagnerdb.ButtonLocation as bl  INNER JOIN fagnerdb.MaterialLocation as ml  ON ml.LOCATION = bl.LOCATION  WHERE bl.BUTTON = '%d'";
    char query[128];
        
    //Botao Apertado     
    sprintf(query, INSERT_SQL, i);
    Serial.println("Material CANCELADO.");
        
    // Initiate the query class instance
    MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
    // Execute the query
    cur_mem->execute(query);
    // Note: since there are no results, we do not need to read any data
    // Deleting the cursor also frees up memory used
    delete cur_mem;
    Serial.println("Informações Enviadas");
  }

  else if(local == 0 && outro == 1){
    char INSERT_SQL[] = "INSERT INTO fagnerdb.jablog  SELECT bl.LOCATION, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP,  ml.MATERIAL, 'Remoto', 'Atendido'  FROM fagnerdb.ButtonLocation as bl  INNER JOIN fagnerdb.MaterialLocation as ml  ON ml.LOCATION = bl.LOCATION  WHERE bl.BUTTON = '%d'";
    char query[128];
        
    //Botao Apertado     
    sprintf(query, INSERT_SQL, i);
    Serial.println("Material ATENDIDO.");
        
    // Initiate the query class instance
    MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
    // Execute the query
    cur_mem->execute(query);
    // Note: since there are no results, we do not need to read any data
    // Deleting the cursor also frees up memory used
    delete cur_mem;
    Serial.println("Informações Enviadas");
  }
  
  else if(local == 1 && outro == 0){
    char INSERT_SQL[] = "INSERT INTO fagnerdb.jablog  SELECT bl.LOCATION, CURRENT_TIMESTAMP, CURRENT_TIMESTAMP,  ml.MATERIAL, 'Local', 'Pedido'    FROM fagnerdb.ButtonLocation as bl  INNER JOIN fagnerdb.MaterialLocation as ml  ON ml.LOCATION = bl.LOCATION  WHERE bl.BUTTON = '%d'";
    char query[128];
        
    //Botao Apertado     
    sprintf(query, INSERT_SQL, i);
    Serial.println("Material PEDIDO.");
        
    // Initiate the query class instance
    MySQL_Cursor *cur_mem = new MySQL_Cursor(&conn);
    // Execute the query
    cur_mem->execute(query);
    // Note: since there are no results, we do not need to read any data
    // Deleting the cursor also frees up memory used
    delete cur_mem;
    Serial.println("Informações Enviadas");
  }
}
