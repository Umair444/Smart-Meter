
/* OVERVIEW:
   I have a variable of current signals, first I have to convert it into voltages.


   PINS of NANO:
   A0 -- Input Value of Current Signals
   A1 -- Input Voltage measurement pin from Transformer Circuit
*/


#include "ThingSpeak.h"
#include "WiFiEsp.h"
#include "secrets.h"

#include <EEPROM.h>
#include <Arduino.h>
#include <U8g2lib.h>

#include <Wire.h>
#include "RTClib.h"

RTC_DS1307 rtc;
//char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
///////////////////////////////

char ssid[] = SECRET_SSID;   // your network SSID (name) 
char pass[] = SECRET_PASS;   // your network password
int keyIndex = 0;            // your network key Index number (needed only for WEP)
WiFiEspClient  client;

// Emulate Serial1 on pins 6/7 if not present
#ifndef HAVE_HWSERIAL1
#include <SoftwareSerial.h>
SoftwareSerial Serial1(2,3); // RX, TX
#define ESP_BAUDRATE  19200
#else
#define ESP_BAUDRATE  115200
#endif

unsigned long myChannelNumber = SECRET_CH_ID;
const char * myWriteAPIKey = SECRET_WRITE_APIKEY;
String myStatus = "";

void thingSpeak_init();
void thingSpeak_update(int v1,int v2,int v3,int v4);
void setEspBaudRate(unsigned long baudrate);
///////////////////////////////////////////////////
const int unitPrice=27;
int totalBill=0;
//////////////////////////////////////////////////////
int memoryAddress=0;
int valueSave    =0;
//////////////////////////////////////////////////////
#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif
#define volt_S A0                // voltage from sensor ACS712
#define volt_IP A1               // Input voltage measured through combination circuit

#define outRelay 7


boolean overLoad=0;

//-----------------------------------Instance Data--------------------------------->
//_________________GLOBALS____________________>
//+++++++LCD++++++++>
U8G2_ST7920_128X64_1_SW_SPI u8g2(U8G2_R0, /* E=*/ 13, /* data=*/ 11, /* R/W=*/ 10, /* reset=*/ 8);
//int ISC = 0;    //InitialScreen Count

//+++Calculations+++>
float avg_v=0,avg_i=0,avg_p=0,avg_e=0,t_energy=0, KWH = 0;

//const float VCC   = 5.0;// supply voltage is from 4.5 to 5.5V. Normally 5V.
//const int model = 2;   // enter the model number (see below)
const uint8_t cutOffLimit = 1;// set the current which below that value, doesn't matter. Or set 0.5
const uint8_t mVperAmp = 100; // use 100 for 20A Module and 66 for 30A Module


//float voltage;// internal variable for voltage

float ACSVoltage = 0;
float VRMS    = 0;
float AmpsRMS = 0;


//+++Calculations+++<

//+++++++Time+++++++>
unsigned int start_t = 0, end_t = 0;
double time_intervel = 0.0;
//+++++++Time+++++++<

////////////////////////////
float IP_voltage();
float IP_curr();
float power(float IP_curr);
float Energy(float IP_curr);
void u8g2_prepare(void);
void Parameters(uint8_t a);
/////////////////////////////
//int getSensorData();
void draw(void);
float getVPP();
float getACS712();
////////////////////////
uint8_t draw_state = 0;
float E_Store = 0;
unsigned int calibration = 75;
unsigned int pF = 90;
float energyCostpermonth = 0;
void setup(){
  Serial.begin(9600);
  thingSpeak_init();
  u8g2.begin();
/////////////////////////////
  pinMode(volt_S, INPUT);
  pinMode(volt_IP, INPUT);
  pinMode(outRelay,OUTPUT);
  digitalWrite(outRelay,HIGH);
/////////////////////////////
  if(! rtc.begin()) {Serial.println("Couldn't find RTC");while (1);}
  if(! rtc.isrunning()){
   Serial.println("RTC is NOT running!");
//   rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
}
//   rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

/*
//////////////////////////////
  u8g2.firstPage();
  for (int i = 0; i < 10; i++) {
    do {
      u8g2.setFont(u8g2_font_6x10_tf);
      u8g2.drawStr( 50 , 10, "WELCOME");
      u8g2.drawStr( 5, 20, "Smart Energy Meter");
      u8g2.drawStr( 10, 35, "Group Memebers");
      u8g2.setFont(u8g2_font_u8glib_4_tr);
      u8g2.drawStr( 10, 50, "Umair, Muneeb, Sohaib, Abubakar");
    } while (u8g2.nextPage());
    delay(5);
  }
  */

t_energy = EEPROM.get(0,t_energy);

}
////////////////////
void loop(){  
  start_t = millis();
  //++++++++++CALCULATIONS+++++++++++++++
  int n = 10;
  int ni=0;
  float var = 0, curr = 0, store = 0;
 // Serial.println("_______CURRENT DATA_________");
  for (int i = 1; i <= n;i++){
    curr=getACS712();
    ni++;
//    curr = IP_curr();
    //if(curr<0.2){curr=0;}else{ni++;Serial.print(curr);}///remove garbage
    var = curr + store;
    store = var;
  }
//  Serial.println("_______CURRENT DATA_________");
   
  end_t = millis();
  time_intervel = (end_t-start_t)/10;
  Serial.print("Time Intervel in ms:");Serial.println(time_intervel);  
  if(ni==0){avg_i=0;}else{avg_i = var/ni;}///avoid to 0/0=nan
  //////////////////////////////////
  avg_v=IP_voltage();
  avg_p = power(avg_i,avg_v);
  avg_e = Energy(avg_p);//////watt per hour
  t_energy =t_energy +  avg_e;///unit
  totalBill=t_energy*unitPrice;

  if(avg_p>150){overLoad=HIGH;}else{overLoad=LOW;}




  Serial.print(" VOLT   : ");Serial.print(avg_v);
  Serial.print(" Current: ");Serial.print(avg_i);
  Serial.print(" Power : ");Serial.println(avg_p);
  Serial.print(" Unit :");Serial.print(t_energy);
  Serial.print(" Total  Bill: ");Serial.println(totalBill);

  EEPROM.put(0,t_energy);
  //+++++++++LCD DISPLAY+++++++++++++
  //"nextPage" is 8 in this case --> see contractor line reset argument (so for change we have to process 8 statements --> (1/16M)*8 = time for change)
  u8g2.firstPage();
  do {
    draw();
  } while ( u8g2.nextPage() );

  // increase the state
  draw_state++;
  if (draw_state >= 1 * 1) {  //Here, 2 is the number of cases in draw()////1 * 8
    draw_state = 0;
  }
  if(overLoad==HIGH){Serial.println("Over Load");digitalWrite(outRelay,LOW);}
  if(overLoad==LOW){digitalWrite(outRelay,HIGH);}

//++++++++++++++WIFI+++++++++++++++
thingSpeak_update(avg_i*1000,avg_p,t_energy,totalBill);
}
/////////////////////////////////////////////////
////////////////-loop End-///////////////////////
//-------------------------------Voltage, Current, Power, Energy-------------------------------->
float IP_voltage(){  //Input Voltage
    int m = analogRead(volt_IP); // read analog values from pin A1 across capacitor
    int n = (m * .304177);  // converts analog value(x) into input ac supply value using this formula
  
//    Serial.print("   analaog input  " ) ; // specify name to the corresponding value to be printed
//    Serial.print(m) ; // print input analog value on serial monitor
//    Serial.print("   ac voltage  ") ; // specify name to the corresponding value to be printed
//    Serial.print(n) ; // prints the ac value on Serial monitor
//    Serial.println();

//  int n = 220; //For now
  return n;
}
////////////////////////////////////////////
float IP_curr(){
ACSVoltage = getVPP();
VRMS = (ACSVoltage/2.0) *0.707;  //root 2 is 0.707
AmpsRMS = (VRMS * 1000)/mVperAmp;
//////////////////////////////
  return AmpsRMS;
}
float getACS712(){  // for AC
  ACSVoltage = getVPP();
  VRMS = (ACSVoltage/2.0) *0.707; 
  VRMS = VRMS - (calibration / 10000.0);     // calibtrate to zero with slider
  AmpsRMS = (VRMS * 1000)/mVperAmp;
  if((AmpsRMS > -0.2) && (AmpsRMS < 0.2)){  // remove low end chatter
    AmpsRMS = 0.0;
  }
return AmpsRMS;
}
//////////////////////////////////////////////////
float power(float IP_curr, float IP_volt){   //For putting average value of current in loop
  float p = IP_curr * IP_volt;         //live Power
  return p;
}
float Energy(float IP_power){ //Average power will be given to
//  Serial.println();
//  Serial.print("Energy ---->");
  //float kW = IP_power/1000;
  //Serial.print("kW ="); Serial.print(kW);

  //Serial.print("; h ="); Serial.print(h);
 
  //Serial.print("; KWH ="); Serial.println(KWH);

    double W_1 = (IP_power/1000) * (pF / 100.0);////kw
    double h = double(time_intervel) / double(3600);   
    double WH_1 = W_1 * h;
    return WH_1;
}
//-------------------------------Voltage, Current, Power, Energy--------------------------------<
//----------------------------------------------LCD--------------------------------------------->
void u8g2_prepare(void){
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);
}
/////////////////////////////////////////////
void Parameters(uint8_t a){

DateTime now = rtc.now();
Serial.print(now.hour());Serial.print(" : ");Serial.print(now.minute());Serial.print(" : ");Serial.println(now.second());
//now.day();/
//now.month();
//now.year();  


//itoa(speed, tmp_string, 10);
//8g_DrawStr(&u8g, 0, 60, tmp_string);
  
  /*Strings*/
  String str1 = String("V :"+String(avg_v));
  String str2 = String("I :"+String(avg_i));
  String str3 = String("P :"+String(avg_p));
  String str4 = String("U :"+String(t_energy));  
  String str5 = String("B :"+String(totalBill));

  char char_array1[str1.length() + 1];
  str1.toCharArray(char_array1, str1.length() + 1);
  char char_array2[str2.length() + 1];
  str2.toCharArray(char_array2, str2.length() + 1);
  char char_array3[str3.length() + 1];
  str3.toCharArray(char_array3, str3.length() + 1);
  char char_array4[str4.length() + 1];
  str4.toCharArray(char_array4, str4.length() + 1);
  char char_array5[str5.length() + 1];
  str5.toCharArray(char_array5, str5.length() + 1);

  u8g2.drawStr(05 , 5, char_array1);
  u8g2.drawStr(75 , 5, char_array2);
  u8g2.drawStr(05, 20, char_array3);
  u8g2.drawStr(75, 20, char_array4);
  u8g2.drawStr(05, 35, char_array5);

if(overLoad==HIGH)
{
  u8g2.drawStr(50,35,"OVER LOAD");
}



  u8g2.drawStr  (5,50,"T:");
  u8g2.setCursor(15,50);u8g2.print(now.hour(),DEC);
  u8g2.setCursor(25,50);u8g2.print(":");
  u8g2.setCursor(30,50);u8g2.print(now.minute(),DEC);
  u8g2.setCursor(40,50);u8g2.print(":");
  u8g2.setCursor(45,50);u8g2.print(now.second(),DEC);
////////////////
  u8g2.drawStr  (60,50,"D:");
  u8g2.setCursor(75,50);u8g2.print(now.day(),DEC);
  u8g2.setCursor(85,50);u8g2.print(":");
  u8g2.setCursor(90,50);u8g2.print(now.month(),DEC);
  u8g2.setCursor(100,50);u8g2.print(":");
  u8g2.setCursor(105,50);u8g2.print(now.year(),DEC);

  
  u8g2.drawFrame(0, 0, u8g2.getDisplayWidth(), u8g2.getDisplayHeight() );

}
//Right shift of 3 means, each case will run 8 times (including 0) --> 2^3=8 (shifts to right)
//AND operation (FOR SCROLLING) with 7 means, when draw_state=7 --> it gives 0 (NOTE: Increase above 8 (like use 4 bit right shift) to scroll more)
void draw(void){
  u8g2_prepare();
  switch (draw_state >> 3) {
    case 0: Parameters(draw_state & 7); break;
  }
}
////////////////////////////////////////////
float getVPP(){
  float result;
  int readValue;             //value read from the sensor
  int maxValue = 0;          // store max value here
  int minValue = 1024;          // store min value here
  
   uint32_t start_time = millis();
   while((millis()-start_time) < 1000) //sample for 1 Sec
   {
       readValue = analogRead(volt_S);
       // see if you have a new maxValue
      // Serial.print("raw Cur: ");       Serial.println(readValue);
       if (readValue > maxValue) 
       {
           /*record the maximum sensor value*/
           maxValue = readValue;
       }
       if (readValue < minValue) 
       {
           /*record the minimum sensor value*/
           minValue = readValue;
       }
   }
   
   // Subtract min from max
   result = ((maxValue - minValue) * 5.0)/1024.0;
      
   return result;
}
////////////////////////
////////////////////////////
void thingSpeak_update(int v1,int v2,int v3,int v4){
  // Connect or reconnect to WiFi
  if(WiFi.status() != WL_CONNECTED){
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(SECRET_SSID);
    while(WiFi.status() != WL_CONNECTED){
      WiFi.begin(ssid, pass);  // Connect to WPA/WPA2 network. Change this line if using open or WEP network
      Serial.print(".");
      delay(5000);     
    } 
    Serial.println("\nConnected.");
  }

  // set the fields with the values
  ThingSpeak.setField(1,v1);
  ThingSpeak.setField(2,v2);
  ThingSpeak.setField(3,v3);
  ThingSpeak.setField(4,v4);  
  // set the status
  ThingSpeak.setStatus(myStatus);  
  // write to the ThingSpeak channel
  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
//  if(x == 200){Serial.println("Channel update successful.");}
//  else{Serial.println("Problem updating channel. HTTP error code " + String(x));}

}
/////////////////////////
void thingSpeak_init(){
  // initialize serial for ESP module  
  setEspBaudRate(ESP_BAUDRATE);  
  while (!Serial){; // wait for serial port to connect. Needed for Leonardo native USB port only
 }
 // Serial.print("Searching for ESP8266..."); 
  //initialize ESP module
  
  WiFi.init(&Serial1);
  // check for the presence of the shield
  if (WiFi.status() == WL_NO_SHIELD){
    Serial.println("WiFi shield not present");
    // don't continue
    while (true);
  }
  Serial.println("found it!");
   
  ThingSpeak.begin(client);  // Initialize ThingSpeak
}
////////////////
// can use 115200, otherwise software serial is limited to 19200.
void setEspBaudRate(unsigned long baudrate){
  long rates[6] = {115200,74880,57600,38400,19200,9600};
  Serial.print("Setting ESP8266 baudrate to ");
  Serial.print(baudrate);
  Serial.println("...");

  for(int i = 0; i < 6; i++){
    Serial1.begin(rates[i]);
    delay(100);
    Serial1.print("AT+UART_DEF=");
    Serial1.print(baudrate);
    Serial1.print(",8,1,0,0\r\n");
    delay(100);  
  }    
  Serial1.begin(baudrate);

}
 
