#include <ArduinoJson.h>              
#include <PubSubClient.h>             // Protocolo MQTT
#include <ESP8266WiFi.h>              // Include the Wi-Fi-Multi library
#include <DHT.h>                      // Include DHT11 
#include <Adafruit_Sensor.h>          // Dependencia de DHT.h
#include <Wire.h>                   //libreria secundaria para LCD
#include <LiquidCrystal_I2C.h>      //libreria del LCD por i2c

#define WIFI_AP ""         // Nombre del wifi
#define WIFI_PASSWORD ""  // Contraseña del wifi
#define TOKEN ""    // ThingsBoard Token
//#define LED_AZUL 0
#define VENTILADOR 15// d8 y 15 es el mismo PIN.lo dejo asi porque no tengo conectado el ventilador secundario y por no cambiar el script.
//#define VENTILADOR 15
#define DHTPIN D3
#define LEDVERDE D4
#define LEDAMARILLO D5
#define LEDROJO D6
#define BUZZER D7
#define DHTTYPE DHT11
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

char thingsboardServer[] = "demo.thingsboard.io";
WiFiClient wifiClient;
PubSubClient client(wifiClient);

// ponemos los gpio en estado LOW
boolean gpioState[] = {false, false};

// Inicializamos el sensor DHT
DHT dht(DHTPIN, DHTTYPE);

int status = WL_IDLE_STATUS;
unsigned long lastSend;
void setup() {
  Serial.begin(115200);
  
  // Set output mode for all GPIO pins
  pinMode(VENTILADOR, OUTPUT);
  pinMode(LEDVERDE, OUTPUT);
  pinMode(LEDAMARILLO, OUTPUT);
  pinMode(LEDROJO, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  delay(10);
  InitWiFi();
  dht.begin();
  lcd.begin(16, 2);
  lcd.clear();
  client.setServer( thingsboardServer, 1883 );
  client.setCallback(on_message);
  lastSend = 0;
}

void loop() {
  if ( !client.connected() ) {
    Serial.println("reconectando");
    reconnect();
  }
    //if ( millis() - lastSend > 2000 ) { // se actualiza cada 2 seg
    apagarLEDs();
    getAndSendTemperatureAndHumidityData();
    float humedad = dht.readHumidity();
    float temperatura = dht.readTemperature();
    lcd.clear(); // Borra pantalla
    lcd.setCursor(0,0); // Inicio del cursor
    lcd.print("Temp          C ");
    lcd.setCursor(8, 0);
    lcd.print(temperatura);
    lcd.setCursor(0,1); // Siguiente renglón.
    lcd.print("Humedad       % ");
    lcd.setCursor(8,1);
    lcd.print(humedad);
    delay(2000);
    //lastSend = millis();
    //} 
  client.loop();
}
void apagarLEDs()
{
  // Apagamos todos los LEDs
  digitalWrite(VENTILADOR, LOW);
  digitalWrite(LEDVERDE, LOW);
  digitalWrite(LEDAMARILLO, LOW);
  digitalWrite(LEDROJO, LOW);
}  
void getAndSendTemperatureAndHumidityData()
{
  Serial.println("Obteniendo datos de temperatura: ");

  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if ((t>15 && t <=30) && (h <= 35)){
  digitalWrite(LEDVERDE, HIGH);
  }
  if ((t>30 && t <=40) && (h >35 && h <60)){
  digitalWrite(LEDAMARILLO, HIGH);
  }
  if (t>40 && h >= 60){
  digitalWrite(LEDROJO, HIGH);
  tone(BUZZER, 2800, 250);
  digitalWrite(VENTILADOR, HIGH);
  }
  
  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    Serial.println("Fallo al leer el sensor DHT!");
    return;
  }
  Serial.print("Humedad: ");
  Serial.print(h);
  Serial.print(" %\t");
  Serial.print("Temperatura: ");
  Serial.print(t);
  Serial.print("ºC ");
  
  String temperature = String(t);
  String humidity = String(h);


  // Just debug messages
  Serial.print( "Enviando telemetria... " );
  // Prepare a JSON payload string
  String payload = "{";
  payload += "\"temperatura\":"; payload += temperature; payload += ",";
  payload += "\"humedad\":"; payload += humidity;
  payload += "}";

  // Enviando datos a la plataforma usando json
  char attributes[100];
  payload.toCharArray( attributes, 100 );
  client.publish( "v1/devices/me/telemetry", attributes );
  Serial.println( attributes );
}

// The callback for when a PUBLISH message is received from the server.
void on_message(const char* topic, byte* payload, unsigned int length) {

  Serial.println("On message");

  char json[length + 1];
  strncpy (json, (char*)payload, length);
  json[length] = '\0';

  Serial.print("Topic: ");
  Serial.println(topic);
  Serial.print("Message: ");
  Serial.println(json);

  // Decode JSON request
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.parseObject((char*)json);

  if (!data.success())
  {
    Serial.println("parseObject() failed");
    return;
  }

  // Check request method
  String methodName = String((const char*)data["method"]);

  if (methodName.equals("getGpioStatus")) {
    // Reply with GPIO status
    String responseTopic = String(topic);
    responseTopic.replace("request", "response");
    client.publish(responseTopic.c_str(), get_gpio_status().c_str());
  } else if (methodName.equals("setGpioStatus")) {
    // Update GPIO status and reply
    set_gpio_status(data["params"]["pin"], data["params"]["enabled"]);
    String responseTopic = String(topic);
    responseTopic.replace("request", "response");
    client.publish(responseTopic.c_str(), get_gpio_status().c_str());
    client.publish("v1/devices/me/attributes", get_gpio_status().c_str());
  }
}

String get_gpio_status() {
  // Prepare gpios JSON payload string
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& data = jsonBuffer.createObject();
  //data[String(LED_AZUL)] = gpioState[0] ? true : false;
  data[String(VENTILADOR)] = gpioState[0] ? true : false;
  char payload[256];
  data.printTo(payload, sizeof(payload));
  String strPayload = String(payload);
  Serial.print("Get gpio status: ");
  Serial.println(strPayload);
  return strPayload;
}
void set_gpio_status(int pin, boolean enabled) {

  if (pin == VENTILADOR) {
    // Output GPIOs state
    digitalWrite(VENTILADOR, enabled ? HIGH : LOW);
    // Update GPIOs state
    gpioState[0] = enabled;
  }
  
}

void InitWiFi() {
  Serial.println("Connecting to AP ...");
  // attempt to connect to WiFi network

  WiFi.begin(WIFI_AP, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connectado al AP");
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    status = WiFi.status();
    if ( status != WL_CONNECTED) {
      WiFi.begin(WIFI_AP, WIFI_PASSWORD);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
      Serial.println("Connectado al AP");
    }
    Serial.print("Conectando a ThingsBoard node ...");
    // Attempt to connect (clientId, username, password)
    if ( client.connect("ESP8266 Device", TOKEN, NULL) ) {
      Serial.println( "[DONE]" );
      // Subscribing to receive RPC requests
      client.subscribe("v1/devices/me/rpc/request/+");
      // Sending current GPIO status
      Serial.println("Enviando estado  GPIO actual ...");
      client.publish("v1/devices/me/attributes", get_gpio_status().c_str());
    } else {
      Serial.print( "[FAILED] [ rc = " );
      Serial.print( client.state() );
      Serial.println( " : intentando en 5 segundos]" );
      // Wait 5 seconds before retrying
      delay( 5000 );
    }
  }
}
