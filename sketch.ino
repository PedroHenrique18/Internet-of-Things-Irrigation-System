#include <stdlib.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#include <DHT.h>
#include <CTBot.h>
#include <SD.h>
#include <SPIFFS.h>
#include <FS.h>
#include <queue>
#include <NTPClient.h>

#define LED 2

std::queue<String> filaDeStrings;

String path, state;
String fileName = "/Logs.txt";
int maxLines = 10;

WiFiUDP udp;
NTPClient ntp(udp, "a.st1.ntp.br", -3 * 3600, 60000);
String hora;

void writeFile(String state, String path, String hora);
void readFile(String path);
void formatFile();
void openFS();

#define DHT_PIN0 15
#define DHTTYPE DHT22
DHT dht01(DHT_PIN0, DHTTYPE);

#define wifi_ssid "Wokwi-GUEST"
#define wifi_password ""
int wifi_timeout = 100000;

CTBot mybot;
String token = "6962103200:AAH0k0c47XpHDpp35SAApbTAWciz4qfAUUU";
const int bot_id = 1854710656;

WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);
#define mqtt_broker "io.adafruit.com"
const int mqtt_port = 1883;
int mqtt_timeout = 10000;
#define mqtt_usernameAdafruitIO "pedroJot4"
#define mqtt_keyAdafruitIO "aio_Lrgu20moOgvLSqPnJaGlLzSES4UD"

Adafruit_MQTT_Client mqtt(&wifi_client, mqtt_broker, mqtt_port, mqtt_usernameAdafruitIO, mqtt_usernameAdafruitIO, mqtt_keyAdafruitIO);
Adafruit_MQTT_Publish temperatura01 = Adafruit_MQTT_Publish(&mqtt, mqtt_usernameAdafruitIO "/feeds/temperatura");
Adafruit_MQTT_Publish umidade01 = Adafruit_MQTT_Publish(&mqtt, mqtt_usernameAdafruitIO "/feeds/umidade");

const char *configFileName = "/morango.txt";
float temperaturaMax;
float temperaturaMin;
float umidadeMin;
float umidadeMax;

void connectWiFi();
void connectMQTT();
void lerConfiguracoes();
void verificarCondicao(float temperatura, float umidade);

void setup()
{
  Serial.begin(9600);

  pinMode(LED, OUTPUT);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  connectWiFi();
  mqtt_client.setServer(mqtt_broker, mqtt_port);

  mybot.wifiConnect(wifi_ssid, wifi_password);
  mybot.setTelegramToken(token);
  if (mybot.testConnection())
  {
    Serial.println("Conectado Telegram");
    mybot.sendMessage(bot_id, "Conectado");
  }
  else
  {
    Serial.println("Falha Telegram");
  }

  ntp.begin();       
  ntp.forceUpdate();

  dht01.begin();
  lerConfiguracoes();
  openFS();
}

void loop()
{
  if (!mqtt_client.connected())
  {
    connectMQTT();
  }
  if (mqtt_client.connected())
  {
    mqtt_client.loop();

    float umidade = dht01.readHumidity();
    float temperatura = dht01.readTemperature();

    if (isnan(umidade) || isnan(temperatura))
    {
      Serial.println(F("Falha ao ler as informações do sensor!"));
      return;
    }

    Serial.print(F("Temperatura: "));
    Serial.print(temperatura);
    Serial.print(F("°C "));
    Serial.print(F("Umidade: "));
    Serial.print(umidade);
    Serial.print(F("% "));
    Serial.println("*********************************");

    temperatura01.publish(temperatura);
    umidade01.publish(umidade);

    verificarCondicao(temperatura, umidade);

    delay(20000);
  }
}

void connectWiFi()
{
  Serial.print("Conectando à rede WiFi .. ");

  unsigned long tempoInicial = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - tempoInicial < wifi_timeout))
  {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Conexão com WiFi falhou!");
  }
  else
  {
    Serial.print("Conectado com o IP: ");
    Serial.println(WiFi.localIP());
  }
}

void connectMQTT()
{
  unsigned long tempoInicial = millis();
  while (!mqtt_client.connected() && (millis() - tempoInicial < mqtt_timeout))
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      connectWiFi();
    }
    Serial.print("Conectando ao MQTT Broker..");

    if (mqtt_client.connect("ESP32Client", mqtt_usernameAdafruitIO, mqtt_keyAdafruitIO))
    {
      Serial.println();
      Serial.print("Conectado ao broker MQTT!");
    }
    else
    {
      Serial.println();
      Serial.print("Conexão com o broker MQTT falhou!");
      delay(500);
    }
  }
  Serial.println();
}

void lerConfiguracoes()
{
  File configFile = SD.open(configFileName);

  if (configFile)
  {
    while (configFile.available())
    {
      String linha = configFile.readStringUntil('\n');
      if (linha.startsWith("temperaturaMax"))
      {
        temperaturaMax = linha.substring(linha.indexOf(' ') + 1).toFloat();
      }
      else if (linha.startsWith("temperaturaMin"))
      {
        temperaturaMin = linha.substring(linha.indexOf(' ') + 1).toFloat();
      }
      else if (linha.startsWith("umidadeMin"))
      {
        umidadeMin = linha.substring(linha.indexOf(' ') + 1).toFloat();
      }
      else if (linha.startsWith("umidadeMax"))
      {
        umidadeMax = linha.substring(linha.indexOf(' ') + 1).toFloat();
      }
    }
    configFile.close();
  }
  else
  {
    Serial.println("Erro ao abrir o arquivo de configuração.");
  }
}

void verificarCondicao(float temperatura, float umidade)
{
  if (umidade < umidadeMin)
  {
    Serial.println("Ligar : umidade baixa");
    mybot.sendMessage(bot_id, "Ligado : umidade baixa");
    hora = ntp.getFormattedTime();
    writeFile("Ligado : umidade baixa", fileName, hora); 
    pinMode(LED, HIGH);
  }
  else if (umidade > umidadeMax)
  {
    Serial.println("Desligar : umidade alta");
    mybot.sendMessage(bot_id, "Desligar : umidade alta");
    hora = ntp.getFormattedTime();
    writeFile("Desligar : umidade alta", fileName, hora);
    pinMode(LED, LOW);
  }
  else if (temperatura > temperaturaMax)
  {
    Serial.println("Ligar : temperatura alta");
    mybot.sendMessage(bot_id, "Ligado : temperatura alta");
    hora = ntp.getFormattedTime();
    writeFile("Ligado : temperatura alta", fileName, hora);
    pinMode(LED, HIGH);
  }
  else if (temperatura < temperaturaMin)
  {
    Serial.println("Desligar : temperatura baixa");
    mybot.sendMessage(bot_id, "Desligar : temperatura alta");
    hora = ntp.getFormattedTime();
    writeFile("Desligar : temperatura alta", fileName, hora);
    pinMode(LED, LOW);
  }
}

void writeFile(String state, String path, String hora)
{
    while (!filaDeStrings.empty())
    {
        filaDeStrings.pop();
    }
    readFile(path);
    File rFile = SPIFFS.open(path, "w");
    if (!rFile)
    {
        Serial.println("Erro ao abrir arquivo!");
        return;
    }
    filaDeStrings.push(state + "," + hora);
    while (filaDeStrings.size() > maxLines)
    {
        filaDeStrings.pop();
    }
    while (!filaDeStrings.empty())
    {
        rFile.println(filaDeStrings.front());
        filaDeStrings.pop();
    }
    rFile.close();
}

void readFile(String path)
{
  Serial.println("Read file");
  File rFile = SPIFFS.open(path, "r");
  if (!rFile)
  {
    Serial.println("Erro ao abrir arquivo!");
    return;
  }
  else
  {
    Serial.print("---------- Lendo arquivo ");

    Serial.print(path);

    Serial.println("  ---------");
    while (rFile.position() < rFile.size())
    {
      String line = rFile.readStringUntil('\n');
      Serial.println(line);
      filaDeStrings.push(line);
    }
    rFile.close();
  }
}

void formatFile()
{
  SPIFFS.format();
  Serial.println("Formatou SPIFFS");
}

void openFS(void)
{
  if (!SPIFFS.begin())
    Serial.println("\nErro ao abrir o sistema de arquivos");
  else
    Serial.println("\nSistema de arquivos aberto com sucesso!");
}