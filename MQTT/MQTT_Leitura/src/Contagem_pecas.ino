#include <WiFiManager.h>  // Incluindo a biblioteca WiFiManager
#include <PubSubClient.h>
#include <EEPROM.h>  // Biblioteca para usar a EEPROM
#include <ArduinoOTA.h>  // Biblioteca para Over-the-Air (OTA)

// Protótipos das funções
void saveToEEPROM();
void sendDataToMQTT();
void clearEEPROM();

// Configurações do Wi-Fi
const char* mqtt_user = "iot";
const char* mqtt_password = "])T=k651zLp2";
const char* mqtt_client_id = "ESP32_AcIoT";

// Configurações do Mosquitto (Broker MQTT)
const char* mqtt_server = "172.210.124.6";  // IP do Mosquitto
const int mqtt_port = 1883;                 // Porta padrão (1883 para sem TLS, 8883 para TLS)

WiFiClient espClient;
PubSubClient client(espClient);

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

void setup() {
  Serial.begin(115200);
  pinMode(pinStatus, INPUT);
  pinMode(pinPecas, INPUT);

  // Inicializando a EEPROM
  EEPROM.begin(512);  // Inicializa a EEPROM (capacidade de 512 bytes)

  // Inicializando o WiFiManager
  WiFiManager wifiManager;

  // Inicia o ponto de acesso para configurar Wi-Fi, se necessário
  if (WiFi.status() != WL_CONNECTED) {
    // Se não estiver conectado, então conecta à rede Wi-Fi usando as credenciais fornecidas
    WiFi.begin("SalaTI", "yeeuwk9k"); // Wifi de teste "BIGARATO", "B1g4r4t0!"

    // Espera a conexão Wi-Fi ser estabelecida
    while (WiFi.status() != WL_CONNECTED) {
      delay(1000);
      Serial.println("Tentando conectar ao Wi-Fi...");
    }
    Serial.println("Conectado ao Wi-Fi!");

    // Se a conexão falhar, entra no modo de configuração usando o WiFiManager
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Falha ao conectar ao Wi-Fi!");
      wifiManager.autoConnect("ESP32_Access_Point");  // Cria ponto de acesso se necessário
      Serial.println("Configurando Wi-Fi via Ponto de Acesso");
    }
  }

 // Inicia a funcionalidade ArduinoOTA
  ArduinoOTA.setHostname("ESP32_AcIoT");
  ArduinoOTA.setPassword("admin123");  // Senha para o OTA (se desejar, pode ser configurada)
  ArduinoOTA.setPort(8266);             // Porta padrão para OTA (opcional)
  ArduinoOTA.begin();  // Inicializa a OTA

if (WiFi.status() == WL_CONNECTED) {
   Serial.print("Conectado ao Wi-Fi. IP: ");
   Serial.println(WiFi.localIP());
}

  // Configurar o cliente MQTT
  client.setServer(mqtt_server, mqtt_port);  // Porta padrão do Mosquitto
}

void reconnect() {
  // Reconnect to Wi-Fi if needed
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Reconectando ao Wi-Fi...");
    WiFi.disconnect();
    WiFi.reconnect();  // Tenta reconectar ao Wi-Fi
    delay(2000);  // Aguarda antes de tentar novamente
  }

  // Tenta reconectar ao broker MQTT
  while (!client.connected()) {
    Serial.println("Conectando ao broker MQTT...");
    if (client.connect(mqtt_client_id, mqtt_user, mqtt_password)) {
      Serial.println("Conectado!");
      // client.subscribe("portao/comando"); // Descomente se quiser assinar um tópico
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
  EEPROM.commit();
  Serial.println("EEPROM limpa.");
}

void saveToEEPROM() {
  // Salva os dados na EEPROM
  EEPROM.put(0, tempoLigada);
  EEPROM.put(4, tempoDesligada);
  EEPROM.put(8, contagemPecas);
  EEPROM.commit();  // Grava os dados na memória
  Serial.println("Dados gravados na EEPROM.");
}

void loop() {

  ArduinoOTA.handle();  // Chama a função OTA para permitir atualizações via rede
  // Verificar a conexão com o Wi-Fi
  if (WiFi.status() == WL_CONNECTED) {
    // Se o Wi-Fi estiver conectado e a EEPROM tiver dados para enviar
    if (EEPROM.read(0) != 0 || EEPROM.read(4) != 0 || EEPROM.read(8) != 0) {
      tempoLigada = EEPROM.read(0);
      tempoDesligada = EEPROM.read(4);
      contagemPecas = EEPROM.read(8);
      sendDataToMQTT();
      clearEEPROM();  // Limpa a EEPROM após enviar os dados
    }

    if (!client.connected()) {
      reconnect();  // Tenta reconectar ao broker MQTT
    }
    client.loop();
  } else {
    Serial.println("Está entrando aqui");
    // Se o Wi-Fi estiver desconectado, armazene os dados na EEPROM
    saveToEEPROM();
    WiFi.reconnect();  // Tenta reconectar ao Wifi
    
  }

  // Lê o status da máquina (ligada/desligada)
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
  } else {
    tempoDesligada += millis() - tempoDesligadoInicial;  // Acumula o tempo de parada
  }

  // Simula a contagem de peças (fechamento de contato do CLP)
  if (digitalRead(pinPecas) == HIGH) {
    contagemPecas++;
    Serial.print("Peças fabricadas: ");
    Serial.println(contagemPecas);
  }

  // Enviar os dados para o Mosquitto via MQTT
  if (WiFi.status() == WL_CONNECTED && client.connected()) {
    String payload = "{";
    payload += "\"tempoLigada\": " + String(tempoLigada) + ","; 
    payload += "\"tempoDesligada\": " + String(tempoDesligada) + ","; 
    payload += "\"contagemPecas\": " + String(contagemPecas) + "}";

    client.publish("acovisa/maquina/dados", payload.c_str());
    Serial.println("Dados enviados para o Mosquitto:");
    Serial.println(payload);
  }

  

  delay(40000);  // Envia os dados a cada 10 segundos
}
