/////////////////// INCLUDES OF EXTERNAL LIBRARIES /////////////////////

#include <TaskScheduler.h>                                // LIBRARY FOR TASKS
#include <ESP8266WiFi.h>                                  // LIBRARY FOR WIFI MANAGEMENT
#include <PubSubClient.h>                                 // LIBRARY FOR MQTT PUBLISHING AND SUBSCRIBING

/////////////////// CONSTANTS & PINS //////////////////////////////////

// USER DEFINED CONSTANTS THAT MUST BE CHANGED:
#define WIFI_SSID "ssid"
#define WIFI_PSWD "password"
#define MQTT_SERVER "test.mosquitto.org"
#define MQTT_TOPIC "bhaek/smart_grower"
// -------------------------


// DEBUG MODE, COMMENT FOLLOWING LINE TO DISABLE
#define DEBUG_MODE
// -------------------------

// PINS -------------------
#define SENSORS A0                                       // USED FOR LIGHT, WATER LEVEL AND MOISTURE SENSORS. SOFTWARE-MULTIPLEXED
#define GROW_LEDS D2
#define WATER_PUMP D1
#define LIGHT_SENSOR_SELECTOR D6                         // THESE THREE PINS ARE USED TO SELECT WICH SENSOR TO READ
#define WATER_SENSOR_SELECTOR D7
#define MOISTURE_SENSOR_SELECTOR D8

// -------------------------

// OTHERS
#define LIGHT_SENSOR 1
#define MOISTURE_SENSOR 2
#define WATER_LEVEL_SENSOR 3
#define LIGHT_CONTROL_SAMPLE_TIME 10                     // IT CONTROLS HOW OFTEN LEDS INTENSITY IS CONTROLLED AND (EVENTUALLY) CHANGED
#define MQTT_UPDATE_TIME 500                             // HOW OFTEN MQTT MESSAGES ARE SENTS (MILLISECONDS)
#define LIGHT_ERROR_THRESHOLD 2                          // USED AS ERROR MARGIN IN LED REGULATION
#define LIGHT_LED_INTENSITY_STEP 2                       // USED WHEN IS NEEDED TO INCREASE/DECREASE LEDS INTENSITY
#define MOISTURE_ERROR_THRESHOLD 100                     // USED AS ERROR MARGIN IN MOISTURE REGULATION
#define WATERING_TIME 3000                               // HOW LONG WATERING PROCESS TAKES
#define MOISTURE_CONTROL_SAMPLE_TIME 60000               // IT CONTROLS HOW OFTEN MOISTURE IS CONTROLLED AND (EVENTUALLY) WATERS THE PLANT
// -------------------------

/////////////////// EXTRA DATA TYPES /////////////////////////////////

enum intensity {zero = 0, one = 1, two = 2};

/////////////////// GLOBAL VARIABLES /////////////////////////////////

// GENERALS

intensity light_intensity = (intensity)0;
int light_sensor_value;
int light_setpoint = 100;
int leds_value = 128;// Initially half power

intensity moisture_intensity = (intensity)0;
int moisture_sensor_value;
int moisture_setpoint = 900;

// SCHEDULER
Scheduler runner;                                       

// WIFI
const char* ssid = WIFI_SSID;                           
const char* password = WIFI_PSWD;                      
WiFiClient smart_grower;

// MQTT
const char* mqtt_server = MQTT_SERVER;         
PubSubClient client(smart_grower);
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];

///////////////////////////////////////////////////////////////////////


/////////////////// FUNCTION DECLARATIONS /////////////////////////////

int read_sensor(int);                                   // THIS FUNCTION READS THE SENSOR ON X PIN

void set_light_intensity(intensity);                    // THESE FUNCTION CHANGE GLOBALLY THE INTENSITY VALUE AND ...
void set_moisture_intensity(intensity);                 // ...  SUBSEQUENTLY THE TARGET VALUE RESPECTIVELY OF LIGHT AND SOIL MOISTURE

void grow_led_control();                                // THIS FUNCTION CONTROLS THE LEDS BASED ON THE LIGHT SENSOR FEEDBACK
void water_control();                                   // THIS FUNCTION CONTROLS THE WATER PUMP BASED ON THE SOIL MOISTURE SENSOR FEEDBACK

int transofrm_water_value(int);                         // THIS FUNCTION FIXS THE WATER SENSOR VALUE TO HAVE A SCLAED 0-100% VALUE      

void setup_wifi();                                      // THIS FUNCTION INITIALIZES THE WIFI CONNECTION

void mtqq_client_main_loop();                           // THESE FUNCTIONS TRY TO CONNECT AND RECONNECT TO THE MQTT SERVER
void reconnect();
  
void message_received(char*, byte*, unsigned int);      // THIS FUNCTION HANDLES THE MESSAGES RECEIVED FROM THE MQTT SERVER 
void send_mtqq_updates();                               // THIS FUNCTION, PERIODICALLY CALLED, SENDS MQTT MESSAGES WITH STATUS UPDATES

/////////////////// TASKS CREATION ///////////////////////////////////

// THIS TASK CHECKS LIGHT INTENSITY AND REGULATES LEDs' POWER
Task grow_led_control_task(LIGHT_CONTROL_SAMPLE_TIME, TASK_FOREVER, &grow_led_control);

// THIS TASK CHECKS MOISTURE INTENSITY AND EVENTUALLY WATERS THE PLANT
Task water_pump_control_task(MOISTURE_CONTROL_SAMPLE_TIME, TASK_FOREVER, &water_control);

// THIS TASK SENDS MQTT UPDATES
Task send_mtqq_updates_task(MQTT_UPDATE_TIME, TASK_FOREVER, &send_mtqq_updates);

// THIS TASK MANAGES MQTT INBOUND
Task mtqq_client_main_loop_task(0, TASK_FOREVER, mtqq_client_main_loop);

/////////////////// FUNCTIONS ////////////////////////////////////////

void setup_wifi() {

  delay(10);
#ifdef DEBUG_MODE
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
#endif
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
#ifdef DEBUG_MODE
    Serial.print(".");
#endif
  }

  randomSeed(micros());
#ifdef DEBUG_MODE
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
#endif
}

void mtqq_client_main_loop() {
  if (!client.connected()) {
#ifdef DEBUG_MODE
    Serial.println("I'M NOT CONNECTED TO THE SERVER...");
#endif
    reconnect();
  }
  client.loop();
}

void send_mtqq_updates() {
  snprintf (msg, MSG_BUFFER_SIZE, "%ld", (int)light_sensor_value);                    // SEND LIGHT SENSOR VALUE
  client.publish(strcat(MQTT_TOPIC,"/status/light_sensor"), msg);
  snprintf (msg, MSG_BUFFER_SIZE, "%ld", (int)light_intensity);                       // SEND LIGHT INTENSITY VALUE
  client.publish(strcat(MQTT_TOPIC,"/status/light_intesnity"), msg);
  snprintf (msg, MSG_BUFFER_SIZE, "%ld", (int)read_sensor(MOISTURE_SENSOR));          // SEND MOISTURE SENSOR VALUE
  client.publish(strcat(MQTT_TOPIC,"/status/moisture_sensor"), msg);
  snprintf (msg, MSG_BUFFER_SIZE, "%ld", (int)moisture_intensity);                    // SEND MOISTURE INTENSITY VALUE
  client.publish(strcat(MQTT_TOPIC,"/status/moisture_intensity"), msg);
  snprintf (msg, MSG_BUFFER_SIZE, "%ld", (int)read_sensor(WATER_LEVEL_SENSOR));       // SEND WATER LEVEL SENSOR VALUE
  client.publish(strcat(MQTT_TOPIC,"/status/water_sensor"), msg);
}

void message_received(char* topic, byte* payload, unsigned int length) {
#ifdef DEBUG_MODE
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
#endif
  String payload_string = "";
  for (int i = 0; i < length; i++) {
#ifdef DEBUG_MODE
    Serial.print((char)payload[i]);
#endif
    payload_string += (char) payload[i];
  }
#ifdef DEBUG_MODE
  Serial.println();
#endif
  if (strcmp(topic, "bhaek/smart_grower/controls/light_intensity") == 0) {              // SET LIGHT INTENSITY MESSAGE ARRIVED
    if (payload_string.compareTo("0") == 0) {
      set_light_intensity(zero);
    } else if (payload_string.compareTo("1") == 0) {
      set_light_intensity(one);
    } else if (payload_string.compareTo("2") == 0) {
      set_light_intensity(two);
    }
  } else if (strcmp(topic, "bhaek/smart_grower/controls/moisture_intensity") == 0) {    // SET MOISTURE INTENSITY MESSAGE ARRIVED
    if (payload_string.compareTo("0") == 0) {
      set_moisture_intensity(zero);
    } else if (payload_string.compareTo("1") == 0) {
      set_moisture_intensity(one);
    } else if (payload_string.compareTo("2") == 0) {
      set_moisture_intensity(two);
    }
  }
}

void reconnect() {
  while (!client.connected()) {
#ifdef DEBUG_MODE
    Serial.print("Attempting MQTT connection...");
#endif
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
#ifdef DEBUG_MODE
      Serial.println("connected");
#endif
      client.subscribe(strcat(MQTT_TOPIC, "/controls/#"));            
    } else {
#ifdef DEBUG_MODE
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
#endif
      delay(5000);                                                // WAIT FOR 5 SECONDS BEFORE RETRY CONNECTION
    }
  }
}

int read_sensor(int sensor) {
  int value = -1;                                         // DEFAULT VALUE -1 RETURNED IN CASE OF MISUSE/ERROR
  switch (sensor) {
    case LIGHT_SENSOR:                                    // LIGHT SENSORE READ REQUESTED
      digitalWrite(MOISTURE_SENSOR_SELECTOR, LOW);
      digitalWrite(WATER_SENSOR_SELECTOR, LOW);
      digitalWrite(LIGHT_SENSOR_SELECTOR, HIGH);
      value = analogRead(SENSORS);
      break;
    case MOISTURE_SENSOR:                                 // MOISTURE SENSORE READ REQUESTED
      digitalWrite(LIGHT_SENSOR_SELECTOR, LOW);
      digitalWrite(WATER_SENSOR_SELECTOR, LOW);
      digitalWrite(MOISTURE_SENSOR_SELECTOR, HIGH);
      value = analogRead(SENSORS);
      break;
    case WATER_LEVEL_SENSOR:                              // WATER LEVEL SENSORE READ REQUESTED
      digitalWrite(LIGHT_SENSOR_SELECTOR, LOW);
      digitalWrite(MOISTURE_SENSOR_SELECTOR, LOW);
      digitalWrite(WATER_SENSOR_SELECTOR, HIGH);
      value = analogRead(SENSORS);                        
      value = transofrm_water_value(value);               // READ VALUE IS TRANSFORMED IN 0-100% VALUE
      break;
  }
  
  // AT THE END OF READINGS I CAN TURN ALL SENSORS OFF
  digitalWrite(LIGHT_SENSOR_SELECTOR, LOW);
  digitalWrite(MOISTURE_SENSOR_SELECTOR, LOW);
  digitalWrite(WATER_SENSOR_SELECTOR, LOW);
  
  return value;
}

void set_light_intensity(intensity i) {                                  // HIGH VALUE => MORE LIGHT
  light_intensity = i;
  light_setpoint = 200 + (int)light_intensity * 75;                      // VALUES ARE 200-275-350
#ifdef DEBUG_MODE
  Serial.println("LIGHT INTENSITY CHANGED. NOW INTENSITY: " + String((int)light_intensity) + " AND SETPOINT: " + String(light_setpoint));
#endif
}

void set_moisture_intensity(intensity i) {                                // HIGH VALUE => MORE DRY
  moisture_intensity = i;
  moisture_setpoint = 900 - (int)moisture_intensity * 300;                // VALUES ARE 900-600-300
#ifdef DEBUG_MODE
  Serial.println("MOISTURE INTENSITY CHANGED. NOW INTENSITY: " + String((int)moisture_intensity) + " AND SETPOINT: " + String(moisture_setpoint));
#endif
}

void grow_led_control() {
  light_sensor_value = read_sensor(LIGHT_SENSOR);
  if (light_sensor_value - light_setpoint > LIGHT_ERROR_THRESHOLD && leds_value > 0) {            // LIGHT IS TOO HIGH AND LEDs ARE ON
    leds_value -= LIGHT_LED_INTENSITY_STEP;
    analogWrite(GROW_LEDS, leds_value);
  } else if (light_sensor_value - light_setpoint < LIGHT_ERROR_THRESHOLD && leds_value < 255) {   // LIGHT IS TOO LOW AND LEDs ARE NOT FULLY ON
    leds_value += LIGHT_LED_INTENSITY_STEP;
    analogWrite(GROW_LEDS, leds_value);
  }
}

void water_control() {                                                                // HIGH IS DRY, LOW IS WET
  moisture_sensor_value = read_sensor(MOISTURE_SENSOR);
  if (moisture_sensor_value - moisture_setpoint > MOISTURE_ERROR_THRESHOLD) {         // MOISTURE IS TOO DRY
#ifdef DEBUG_MODE
    Serial.println("MOISTURE SENSOR VALUE: " + String(moisture_sensor_value) + " AND SETPOINT: " + String(moisture_setpoint));
#endif
    water_plant();
  }
}

void water_plant() {
#ifdef DEBUG_MODE
  Serial.println("WATER PUMP ON");
#endif
  digitalWrite(WATER_PUMP, HIGH);
  delay(WATERING_TIME);
#ifdef DEBUG_MODE
  Serial.println("WATER PUMP OFF");
#endif
  digitalWrite(WATER_PUMP, LOW);
}

int transofrm_water_value(int value) {
  switch (value) {
    case 890  ... 1024:
      return 0;
      break;
    case 360 ... 889:
      return 10;
      break;
    case 280 ... 359:
      return 20;
      break;
    case 200 ... 279:
      return 30;
      break;
    case 120 ... 199:
      return 40;
      break;
    case 110 ... 119:
      return 50;
      break;
    case 100 ... 109:
      return 60;
      break;
    case 90 ... 99:
      return 70;
      break;
    case 80 ... 89:
      return 80;
      break;
    case 70 ... 79:
      return 90;
      break;
    case 0 ... 69:
      return 100;
      break;
  }

  return value;
}

void setup() {

  analogWriteRange(255);
  analogWriteFreq(40000);

  // PINS SETUP
  pinMode(SENSORS, INPUT);
  pinMode(LIGHT_SENSOR_SELECTOR, OUTPUT);
  pinMode(GROW_LEDS, OUTPUT);
  pinMode(MOISTURE_SENSOR_SELECTOR, OUTPUT);
  pinMode(WATER_SENSOR_SELECTOR, OUTPUT);
  pinMode(WATER_PUMP, OUTPUT);

  // DEBUG SETUP
#ifdef DEBUG_MODE
  Serial.begin(115200);
  Serial.println("DEBUG MODE ON");
#endif

  // WIFI SETTUP
  setup_wifi();

  //MQTT SETUP
  client.setServer(mqtt_server, 1883);
  client.setCallback(message_received);

  // SCHEDULER SETUP
  runner.init();
#ifdef DEBUG_MODE
  Serial.println("SCHEDULER INITIALIZED SUCCESSFULLY");
#endif
  runner.addTask(grow_led_control_task);
  runner.addTask(water_pump_control_task);
  runner.addTask(send_mtqq_updates_task);
  runner.addTask(mtqq_client_main_loop_task);
#ifdef DEBUG_MODE
  Serial.println("TASKS ADDED SUCCESSFULLY");
#endif
  grow_led_control_task.enable();
  water_pump_control_task.enable();
  send_mtqq_updates_task.enable();
  mtqq_client_main_loop_task.enable();
#ifdef DEBUG_MODE
  Serial.println("TASKS ENABLED SUCCESSFULLY");
#endif

#ifdef DEBUG_MODE
  Serial.println("SETUP FINISHED, STARTING MAIN LOOP...");
#endif
}

void loop() {
  runner.execute();
}
