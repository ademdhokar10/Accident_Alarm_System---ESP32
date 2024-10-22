#include<LiquidCrystal_I2C.h>
#include <AltSoftSerial.h>
#include <TinyGPS++.h>
#include <SoftwareSerial.h>
#include <math.h>
#include<Wire.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const String EMERGENCY_PHONE = "+21629572597";

//GSM Module RX pin to Arduino 3
//GSM Module TX pin to Arduino 2
#define rxPin 2
#define txPin 3
SoftwareSerial sim800(rxPin,txPin);

//GPS Module RX pin to Arduino 9
//GPS Module TX pin to Arduino 8
AltSoftSerial neogps;
TinyGPSPlus gps;

String sms_status,sender_number,received_date,msg;
String altitude, longitude;

#define BUZZER 12
#define BUTTON 11

#define xPin A1
#define yPin A2
#define zPin A3


byte updateflag;

int xaxis = 0, yaxis = 0, zaxis = 0;
int deltx = 0, delty = 0, deltz = 0;
int vibration = 2, devibrate = 75;
int magnitude = 0;
int sensitivity = 20;
double angle;
boolean impact_detected = false;
//Used to run impact routine every 2mS.
unsigned long time1;
unsigned long impact_time;
unsigned long alert_delay = 30000;



void setup()
{
  //Serial.println("Arduino serial initialize");
  Serial.begin(9600);
  //Serial.println("SIM800L serial initialize");
  sim800.begin(9600);
  //Serial.println("NEO6M serial initialize");
  neogps.begin(9600);
  //--------------------------------------------------------------
  pinMode(BUZZER, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);
  //--------------------------------------------------------------

  lcd.begin();
  lcd.backlight();
  lcd.clear();

  sms_status = "";
  sender_number="";
  received_date="";
  msg="";

  sim800.println("AT"); //Check GSM Module
  delay(1000);
  sim800.println("ATE1"); //Echo ON
  delay(1000);
  sim800.println("AT+CPIN?"); //Check SIM ready
  delay(1000);
  sim800.println("AT+CMGF=1"); //SMS text mode
  delay(1000);
  sim800.println("AT+CNMI=1,1,0,0,0"); 
  delay(1000);

  time1 = micros(); 

  xaxis = analogRead(xPin);
  yaxis = analogRead(yPin);
  zaxis = analogRead(zPin);

}

void loop()
{

  //call impact routine every 2mS
  if (micros() - time1 > 1999) Impact();
  if(updateflag > 0) 
  {
    updateflag=0;
    Serial.println("Impact detected!!");
    Serial.print("Magnitude:"); Serial.println(magnitude);

    getGps();
    digitalWrite(BUZZER, HIGH);
    impact_detected = true;
    impact_time = millis();
    
    lcd.clear();
    lcd.setCursor(0,0); 
    lcd.print("Crash Detected");
    lcd.setCursor(0,1); 
    lcd.print("Magnitude:"+String(magnitude));
  }
  if(impact_detected == true)
  {
    if(millis() - impact_time >= alert_delay) {
      digitalWrite(BUZZER, LOW);
      makeCall();
      delay(1000);
      sendAlert();
      impact_detected = false;
      impact_time = 0;
    }
  }
  
  if(digitalRead(BUTTON) == LOW){
    delay(200);
    digitalWrite(BUZZER, LOW);
    impact_detected = false;
    impact_time = 0;
  }
  while(sim800.available()){
    parseData(sim800.readString());
  }
  while(Serial.available())  {
    sim800.println(Serial.readString());
  }


}

void Impact()
{

  time1 = micros(); 
  int oldx = xaxis; 
  int oldy = yaxis;
  int oldz = zaxis;
  xaxis = analogRead(xPin);
  yaxis = analogRead(yPin);
  zaxis = analogRead(zPin);
  
  //--------------------------------------------------------------
  //Loop counter prevents false triggering.
  vibration--; 
  if(vibration < 0) vibration = 0;                                
  if(vibration > 0) return;
  deltx = xaxis - oldx;                                           
  delty = yaxis - oldy;
  deltz = zaxis - oldz;
  magnitude = sqrt(sq(deltx) + sq(delty) + sq(deltz));

  if (magnitude >= sensitivity) //impact detected
  {
    updateflag=1;
    vibration = devibrate;
  }
  else
  {
    magnitude=0;
  }
}
//-------------------------------------------------------------
void parseData(String buff){
  Serial.println(buff);
  unsigned int len, index;
  index = buff.indexOf("\r");
  buff.remove(0, index+2);
  buff.trim();
  if(buff != "OK"){
    index = buff.indexOf(":");
    String cmd = buff.substring(0, index);
    cmd.trim();
    buff.remove(0, index+2);
    if(cmd == "+CMTI"){
      index = buff.indexOf(",");
      String temp = buff.substring(index+1, buff.length()); 
      temp = "AT+CMGR=" + temp + "\r"; 
      sim800.println(temp); 
    }

    else if(cmd == "+CMGR"){
      if(buff.indexOf(EMERGENCY_PHONE) > 1){
        buff.toLowerCase();
        if(buff.indexOf("get gps") > 1){
          getGps();
          String sms_data;
          sms_data = "GPS Location Data\r";
          sms_data += "http://maps.google.com/maps?q=loc:";
          sms_data += altitude + "," + longitude;
          sendSms(sms_data);
        }
      }
    }
  }
  else{
  }
}
//----------------------------------------------------
void getGps()
{
  boolean newData = false;
  for (unsigned long start = millis(); millis() - start < 2000;){
    while (neogps.available()){
      if (gps.encode(neogps.read())){
        newData = true;
        break;
      }
    }
  }
  
  if (newData)
  {
    altitude = String(gps.location.lat(), 6);
    longitude = String(gps.location.lng(), 6);
    newData = false;
  }
  else {
    Serial.println("No GPS data is available");
    altitude = "";
    longitude = "";
  }

  Serial.print("Altitude= "); Serial.println(altitude);
  Serial.print("Lngitude= "); Serial.println(longitude);
}
//----------------------------------------------------
void sendAlert()
{
  String sms_data;
  sms_data = "Accident Alert!!\r";
  sms_data += "http://maps.google.com/maps?q=loc:";
  sms_data += altitude + "," + longitude;
  sendSms(sms_data);
}
//----------------------------------------------------
void makeCall()
{
  Serial.println("calling....");
  sim800.println("ATD"+EMERGENCY_PHONE+";");
  delay(20000); 
  sim800.println("ATH");
  delay(1000);
}

 void sendSms(String text)
{s
  sim800.print("AT+CMGF=1\r");
  delay(1000);
  sim800.print("AT+CMGS=\""+EMERGENCY_PHONE+"\"\r");
  delay(1000);
  sim800.print(text);
  delay(100);
  sim800.write(0x1A);s
  delay(1000);
  Serial.println("SMS Sent Successfully.");
}

boolean SendAT(String at_command, String expected_answer, unsigned int timeout){

    uint8_t x=0;
    boolean answer=0;
    String response;
    unsigned long previous;
    while( sim800.available() > 0) sim800.read();
    sim800.println(at_command);
    x = 0;
    previous = millis();
    do{s
        if(sim800.available() != 0){
            response += sim800.read();
            x++;
            if(response.indexOf(expected_answer) > 0){
                answer = 1;
                break;
            }
        }
    }while((answer == 0) && ((millis() - previous) < timeout));

  Serial.println(response);
  return answer;
}
