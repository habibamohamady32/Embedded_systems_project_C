#include <LiquidCrystal.h>
#include <SoftwareSerial.h>
#define DEBUG true
#define smoke_threshold 20
#define temp_threshold 30
const char* ssid = "12345678";
const char* password = "12345678";

const int ledPin1 = 13;
const int ledPin2 = 12;
SoftwareSerial espSerial(10, 11); // RX, TX

boolean ledOn = false;

// initialize the library with the numbers of the interface pins
//               rs, en, d4, d5, d6, d7
LiquidCrystal lcd(9, 8, 6, 5, 4, 3);

int pirPin = 2;       // PIR motion sensor
int smokePin = 1;    // MQ2 smoke sensor A1
int tempPin = 0;     // LM35 temperature sensor A0
int pirState = 0; // initialize the PIR state as no motion
float tempValue = 0; // initialize the smoke value as 0
int smokeValue = 0; // initialize the smoke value as no smoke

void setup() {
  Serial.begin(115200);
  delay(10);

  espSerial.begin(115200);

  pinMode(ledPin1, OUTPUT);
  digitalWrite(ledPin1, LOW);
  pinMode(ledPin2, OUTPUT);
  digitalWrite(ledPin2, LOW);

  Serial.println("");
  Serial.println("Waiting for ESP8266 to respond...");

  espSerial.println("AT+RST");
  delay(1000);

  if (espSerial.find("ready")) {
    Serial.println("ESP8266 is ready");
  } else {
    Serial.println("ESP8266 is not responding");
    while (1);
  }

  connectWiFi();
  startServer();

  // set up the LCD's number of columns and rows
  lcd.begin(16, 2);
  // initialize serial communication
  //Serial.begin(9600);
  // set up the PIR motion sensor as input
  pinMode(pirPin, INPUT);
  // configuration for timer
  TCCR1A = 0;
  TCCR1B = (1 << CS10) | (1 << CS12); // set prescaler to 1024
  // configuration for interrupt
  INIT_interrupt();
  // configuration for ADC function
  INIT_ADC();
}

void loop() {
  if (espSerial.available()) {
    Serial.write(espSerial.read());
  }

  if (Serial.available()) {
    espSerial.write(Serial.read());
  }

  if (espSerial.find("+IPD,")) {
    int connectionId = espSerial.read() - '0';
    espSerial.find("pin=");
    int pinValue = espSerial.read() - '0';

    if (pinValue == 1) {
      digitalWrite(ledPin1, HIGH);
      digitalWrite(ledPin2, HIGH);
      ledOn = true;
    } else if (pinValue == 0) {
      digitalWrite(ledPin1, LOW);
      digitalWrite(ledPin2, LOW);
      ledOn = false;
    }

    String response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: text/html\r\n\r\n";
    response += "<!DOCTYPE HTML>\r\n<html>\r\n";
    response += "<body>\r\n";
    response += "LED is currently " + String(ledOn ? "<strong>ON</strong>" : "<strong>OFF</strong>") + "<br><br>";
    response += "<form><input type=\"button\" value=\"Toggle LED\" onclick=\"toggleLED()\"></form>\r\n";
    response += "<script>function toggleLED() { var xhr = new XMLHttpRequest(); xhr.open('GET', '/toggle?pin=' + (Number(!" + String(ledOn) + ")), true); xhr.send(); }</script>\r\n";
     if (smokeValue > smoke_threshold) {
      response += "<p style=\"color: red;\">ALERT THERE IS SMOKE!</p>\r\n";
    } else {
      response += "<p>ALL CLEAR!</p>\r\n";
    }
    if (tempValue > temp_threshold) {
      response += "<p style=\"color: red;\">ALERT Tempreature is HIGH!!!!</p>\r\n";
    } else {
      response += "<p>it is a cool day!</p>\r\n";
    }
    response += "</body>\r\n";
    response += "</html>\r\n";

    String responseLength = String(response.length());

    espSerial.println("AT+CIPSEND=" + String(connectionId) + "," + responseLength);
    if (espSerial.find(">")) {
      espSerial.print(response);
    }
  }
  // print the sensor readings on the LCD
  lcd.clear();
  //pir_read();
  temp_Read();
  smoke_read();
  // wait for 1 second before reading the sensors again
  delay2sec();
}
void pir_read() {
  pirState = digitalRead(pirPin);
  lcd.setCursor(0, 0);
  lcd.print("PIR:");
  lcd.print(pirState);

  //  Serial.print("PIR: ");
  //  Serial.println(pirState);
}
void temp_Read() {
  tempValue = Analog_Read(tempPin) * 0.48875; //convert ADC value to temperature in Celsius
  lcd.setCursor(8, 0); // set the cursor to the first column of the second row
  lcd.print("T:");
  lcd.print(tempValue); // display the temperature value on the LCD
  lcd.print("C"); // display the temperature unit on the LCD

  //  Serial.print("Temp: ");
  //  Serial.print(tempValue);
  //  Serial.println("C");

}
void smoke_read()
{
  smokeValue = Analog_Read(smokePin);
  lcd.setCursor(0, 1); // set the cursor to the first column of the first row
  lcd.print("smoke:"); // display message on the LCD
  lcd.print(smokeValue); // display if there is a smoke or not

  //  Serial.print("Smoke: ");
  //  Serial.println(smokeValue);
}
void delay2sec() {
  TCNT1 = 31249; // set timer count value for 2 seconds delay
  TIFR1 = (1 << TOV1); // clear any pending interrupt
  while ((TIFR1 & (1 << TOV1)) == 0); // wait for timer overflow interrupt
}
void INIT_interrupt()
{
  sei();
  EIMSK |= (1 << INT0); // enable external interrupt
  EICRA |= (1 << ISC01) | (1 << ISC00); // rissing edge
}

void INIT_ADC()
{
  ADMUX |= (1 << REFS0);//vcc
  ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0) | (1 << ADEN); // //enable ADC and adjust clock Division Factor/128
}

int Analog_Read(uint8_t pin_num)
{
  ADMUX |= pin_num;//channel number
  ADCSRA |= (1 << ADSC);// start convertion
  while (ADCSRA & (1 << ADIF) == 0);//wait until convert
  return ADC;// ADCH:ADCL // return converted value
}
ISR(INT0_vect) {
  lcd.clear();
  pir_read();
  delay2sec();
}

void connectWiFi() {
  Serial.println("");
  Serial.println("Connecting to Wi-Fi");

  espSerial.println("AT+CWJAP=\"" + String(ssid) + "\",\"" + String(password) + "\"");
  delay(5000);

  if (espSerial.find("OK")) {
    Serial.println("Connected to Wi-Fi");
  } else {
    Serial.println("Failed to connect to Wi-Fi");
    while (1);
  }
}

void startServer() {
  espSerial.println("AT+CIPMUX=1");
  delay(1000);

  espSerial.println("AT+CIPSERVER=1,80");
  delay(1000);

  if (espSerial.find("OK")) {
    Serial.println("Server started");
    sendData("AT+CIFSR\r\n", 1500, DEBUG);
  } else {
    Serial.println("Failed to start server");
    while (1);
  }
}
String sendData(String command, const int timeout, boolean debug)
{
  String response = "";                                             //initialize a String variable named "response". we will use it later.

  espSerial.print(command);                                           //send the AT command to the esp8266 (from ARDUINO to ESP8266).
  long int time = millis();                                         //get the operating time at this specific moment and save it inside the "time" variable.
  while ( (time + timeout) > millis())                              //excute only whitin 1 second.
  {
    while (espSerial.available())                                     //is there any response came from the ESP8266 and saved in the Arduino input buffer?
    {
      char c = espSerial.read();                                      //if yes, read the next character from the input buffer and save it in the "response" String variable.
      response += c;                                                //append the next character to the response variabl. at the end we will get a string(array of characters) contains the response.
    }
  }
  if (debug)                                                        //if the "debug" variable value is TRUE, print the response on the Serial monitor.
  {
    Serial.println(response);
  }
  return response;                                                  //return the String response.
}
