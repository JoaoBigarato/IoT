#include <SPI.h>
#include <Ethernet.h>  // Biblioteca Ethernet para o Arduino Mega
#include <PubSubClient.h>
#include <EEPROM.h>  // Biblioteca para usar a EEPROM
#include <ArduinoOTA.h>  // Biblioteca para Over-the-Air (OTA)

// Definindo o endereço MAC aqui
byte macAddress[] = { 0xB0, 0x5C, 0xDA, 0x9C, 0x66, 0xC1 };

// Protótipos das funções
void saveToEEPROM();
void sendDataToMQTT();
void clearEEPROM();
void reconnectWiFi();
void connectMQTT();
void calculateOperatingTime();
void sendData(String payload);
String collectData();  // Alterado para retornar payload
void readEEPROM();  // Função para ler os dados da EEPROM

// Configurações do MQTT
const char* mqtt_user = "iot";
const char* mqtt_password = "])T=k651zLp2";
const char* mqtt_client_id = "ESP32_AcIoT2";

// Configurações do Mosquitto (Broker MQTT)
const char* mqtt_server = "172.210.124.6";  // IP do Mosquitto
const int mqtt_port = 1883;                 // Porta padrão (1883 para sem TLS, 8883 para TLS)

EthernetClient ethClient;  // Cliente Ethernet
PubSubClient client(ethClient);

// Pinos de leitura
int pinStatus = 13;  // Pino que monitora o status da máquina (ligada/desligada)
int pinPecas = 12;   // Pino que lê a contagem de peças do CLP

// Variáveis de controle
bool maquinaLigada = false;
unsigned long tempoLigada = 0;
unsigned long tempoDesligada = 0;
unsigned long tempoLigadoInicial = 0;  // Marca o início do tempo ligado
unsigned long tempoDesligadoInicial = 0;  // Marca o início do tempo desligado
int contagemPecas = 0;
bool estadoAnterioPecas = false;  // Inicializado corretamente

unsigned long lastSendTime = 0;  // Armazena o último tempo em que as informações foram enviadas
const unsigned long sendInterval = 5000;  // Intervalo de envio em milissegundos (5 segundos, por exemplo)

void setup() {
  Serial.begin(115200);
  pinMode(pinStatus, INPUT);
  pinMode(pinPecas, INPUT);

  // Inicializando a EEPROM

  // Inicializando o Ethernet Shield
  Ethernet.begin(macAddress);  // Inicia a Ethernet com o endereço MAC

  Serial.println("Pronto para OTA!");

 // Conectar ao servidor MQTT
  client.setServer(mqtt_server, mqtt_port);  // Porta padrão do Mosquitto
  Serial.println("Conectando ao servidor MQTT...");

  // Ler os dados da EEPROM (restaurando valores persistentes)
  readEEPROM();
  Serial.println("EEPROM lida.");
  // Configurar o cliente MQTT
  client.setServer(mqtt_server, mqtt_port);  // Porta padrão do Mosquitto
}

void loop() {
  ArduinoOTA.handle();  // Verifica atualizações OTA
  
  // Reconectar ao Wi-Fi se necessário
  reconnectWiFi();
  
  // Verificar e calcular o tempo de operação
  calculateOperatingTime();

  bool sendDataNow = (millis() - lastSendTime) >= sendInterval;  // Verifica se chegou o momento de enviar os dados
  if (sendDataNow) {
    // Coleta os dados e os envia
    String payload = collectData();
    sendData(payload);
    lastSendTime = millis();  // Atualiza o tempo do último envio
  }

  // Conectar ao MQTT se necessário
  if (!client.connected()) {
    connectMQTT();
  }

  client.loop();  // Processa as mensagens MQTT
}

void reconnectWiFi() {
  if (Ethernet.linkStatus() == LinkOFF) {  // Verifica a conexão de rede
    Serial.println("Reconectando ao Ethernet...");
    Ethernet.begin(macAddress);  // Conecta novamente
    
    delay(2000);
    if (Ethernet.linkStatus() == LinkON) {
      Serial.println("Conexão Ethernet estabelecida!");
    } else {
      Serial.println("Falha ao reconectar no Ethernet!");
    }
  }
}

void connectMQTT() {
  while (!client.connected()) {
    Serial.println("Conectando ao broker MQTT...");
    if (client.connect(mqtt_client_id, mqtt_user, mqtt_password)) {
      Serial.println("Conectado ao MQTT!");
    } else {
      Serial.print("Falha, rc=");
      Serial.print(client.state());
      Serial.println(" Tentando novamente em 1 segundo...");
      delay(1000);
    }
  }
}

void sendDataToMQTT() {
  String payload = "{";
  payload += "\"tempoLigada\": " + String(tempoLigada / 1000) + ",";  // Convertendo para segundos
  payload += "\"tempoDesligada\": " + String(tempoDesligada / 1000) + ",";  // Convertendo para segundos
  payload += "\"contagemPecas\": " + String(contagemPecas) + "}";

  client.publish("acovisa/maquina/dados", payload.c_str());
  Serial.println("Dados enviados para o Mosquitto:");
  Serial.println(payload);
}

void clearEEPROM() {
  // Limpa os dados da EEPROM após envio bem-sucedido
  EEPROM.put(0, 0);
  EEPROM.put(4, 0);
  EEPROM.put(8, 0);
  Serial.println("EEPROM limpa.");
}

void saveToEEPROM() {
  // Salva os dados na EEPROM
  EEPROM.put(0, tempoLigada);
  EEPROM.put(4, tempoDesligada);
  EEPROM.put(8, contagemPecas);
  Serial.println("Dados gravados na EEPROM.");
}

void readEEPROM() {
  // Lê os dados da EEPROM
  tempoLigada = EEPROM.read(0);
  tempoDesligada = EEPROM.read(4);
  contagemPecas = EEPROM.read(8);

  if (tempoLigada == 0 && tempoDesligada == 0 && contagemPecas == 0) {
    Serial.println("Nenhum dado encontrado na EEPROM. Iniciando valores padrão.");
  } else {
    Serial.println("Dados restaurados da EEPROM:");
    Serial.print("Tempo Ligado: ");
    Serial.println(tempoLigada);
    Serial.print("Tempo Desligado: ");
    Serial.println(tempoDesligada);
    Serial.print("Contagem de Peças: ");
    Serial.println(contagemPecas);
  }
}

void calculateOperatingTime() {
  bool status = digitalRead(pinStatus);
  if (status == HIGH) {  // Máquina ligada
    if (!maquinaLigada) {
      maquinaLigada = true;
      tempoLigadoInicial = millis();  // Marca o tempo de ligação
    }
  } else {  // Máquina desligada
    if (maquinaLigada) {
      maquinaLigada = false;
      tempoDesligadoInicial = millis();  // Marca o tempo de desligamento
    }
  }

  // Acumular o tempo de máquina ligada e desligada
  if (maquinaLigada) {
    tempoLigada += millis() - tempoLigadoInicial;  // Acumula o tempo de operação
    tempoLigadoInicial = millis();  // Atualiza o tempo inicial para evitar incremento excessivo
  } else {
    tempoDesligada += millis() - tempoDesligadoInicial;  // Acumula o tempo de parada
    tempoDesligadoInicial = millis();  // Atualiza o tempo inicial para evitar incremento excessivo
  }

  bool estadoPinPecas = digitalRead(pinPecas);  // Lê o estado atual do pino

  // Verifica se houve uma transição de LOW para HIGH (indicando fabricação de uma peça)
  if (estadoAnterioPecas == false && estadoPinPecas == HIGH) {
    contagemPecas++;  // Incrementa a contagem de peças
    Serial.print("Peças fabricadas: ");
    Serial.println(contagemPecas);
  }

  // Atualiza o estado anterior do pino para o estado atual
  estadoAnterioPecas = estadoPinPecas;
}

String collectData() {
  String payload = "{";
  payload += "\"tempoLigada\": " + String(tempoLigada) + ",";
  payload += "\"tempoDesligada\": " + String(tempoDesligada) + ",";
  payload += "\"contagemPecas\": " + String(contagemPecas) + "}";
  return payload;  // Agora retorna o payload corretamente
}

void sendData(String payload) {
  // Verifica se está conectado ao Wi-Fi
 if (client.connected()) {
    sendDataToMQTT();
  } else {
    // Caso o Wi-Fi não esteja conectado, salva na EEPROM
    saveToEEPROM();
  }
}
