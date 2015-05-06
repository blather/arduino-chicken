
#include <WiFlyHQ.h>

// to support d[ouble]t[o]str[ing]f[loat]
// because the AVRs sprintf doesn't support 
// floats
#include<stdlib.h>
// for LCD display
#include <SoftwareSerial.h>
// Date, time and temperature functions using 
// a Chronodot RTC connected via I2C and Wire lib
// SDA on analog 4 and SCL on analog 5
#include <Wire.h>
#include "Chronodot.h"
// add lib for reading Dallas 1-Wire DS18x20 devices
#include <OneWire.h>

Chronodot RTC;
OneWire  ds(10);  // on pin 10
// Attach the serial display's RX line to digital pin 2
SoftwareSerial LCD(3,2); // pin 2 = TX, pin 3 = RX (unused)
SoftwareSerial WIFI(9,8); // pin 4 = TX, pin 5 = RX

WiFly wifly;
const char cthulhu[] = "192.168.70.129";

void setup () {
  
    Serial.begin(9600);
    Serial.println("Initializing Chronodot.");
    LCD.begin(9600);
    delay(500); // wait for display to boot up
    LCD.write(254); // cls
    LCD.write(1);
    LCD.write(254); // cursor to beginning of first line
    LCD.write(128);
    LCD.write("initializing");
      
    Wire.begin();
    RTC.begin();
    
    WIFI.begin(9600);
    if (!wifly.begin(&WIFI)) {
        Serial.println("Failed to start wifly");
    }

  if (! RTC.isrunning()) {
    Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    RTC.adjust(DateTime(__DATE__, __TIME__));
  }
  //RTC.adjust(DateTime(__DATE__, __TIME__));
}

void loop () {
      
  byte i;
  byte present = 0;
  byte data[12];
  byte addr[8];
  byte type_s;
  float celsius, fahrenheit;
  char outsidetempstringLCD[8] = (" 0.0"), insidetempstringLCD[8] = (" 0.0"), outsidetempstring[8] = ("0.0"), insidetempstring[8] = ("0.0");
  char buf[32];
  String wifly_time;
  char wiflyTime[5];
  String postmesg = ""; 
  
  DateTime now = RTC.now();
  
//  wifly_time = wifly.getTime(buf, sizeof(buf));
//  wifly_time.toCharArray(wiflyTime, sizeof(wiflyTime));
//  Serial.println(wifly_time);
//  sprintf(buf, "%02d", now.month());
//  crondot_date = buf;
//  crondot_date += " ";
//  sprintf(buf, "%02d", now.day());
//  crondot_date += buf;
//  crondot_date += " ";
//  sprintf(buf, "%4d", now.year());
//  crondot_date += buf;
//  RTC.adjust(DateTime(crondot_date, wifly_time));
  
//  Serial.println(wifly_time);
//  Serial.println("crondot date " + crondot_date);
  
  
  if ( now.minute() % 5 == 0 ) {
//    while ( 1 ) {
    
    LCD.write(254);  // cls
    LCD.write(1);
    
//    if ( !ds.search(addr)) {
//      Serial.print("No more addresses.\n");
//      ds.reset_search();
//      delay(250);
//      return;
//    }
    int j = 0;
    while ( ds.search(addr) ) {
      Serial.print("R=");
      for( i = 0; i < 8; i++) {
        Serial.print(addr[i], HEX);
        Serial.print(" ");
      }
      if ( OneWire::crc8( addr, 7) != addr[7]) {
        Serial.print("CRC is not valid!\n");
        return;
      }
     
      // the first ROM byte indicates which chip
      switch (addr[0]) {
        case 0x10:
          Serial.println("  Chip = DS18S20");  // or old DS1820
          type_s = 1;
          break;
        case 0x28:
          Serial.println("  Chip = DS18B20");
          type_s = 0;
          break;
        case 0x22:
          Serial.println("  Chip = DS1822");
          type_s = 0;
          break;
        default:
          Serial.println("Device is not a DS18x20 family device.");
          return;
      } 
  
      ds.reset();
      ds.select(addr);
      //ds.write(0x44,1);         // start conversion, with parasite power on at the end
      ds.write(0x44);         // start conversion, without parasite power    
      //delay(1000);     // maybe 750ms is enough, maybe not -- not needed if not on parasite power
      
      present = ds.reset();
      ds.select(addr);    
      ds.write(0xBE);         // Read Scratchpad
    
      Serial.print("P=");
      Serial.print(present,HEX);
      Serial.print(" ");
      for ( i = 0; i < 9; i++) {           // we need 9 bytes
        data[i] = ds.read();
        Serial.print(data[i], HEX);
        Serial.print(" ");
      }
      Serial.print(" CRC=");
      Serial.print( OneWire::crc8( data, 8), HEX);
      Serial.println();
      
      // Convert the data to actual temperature
      // because the result is a 16 bit signed integer, it should
      // be stored to an "int16_t" type, which is always 16 bits
      // even when compiled on a 32 bit processor.
      int16_t raw = (data[1] << 8) | data[0];
      if (type_s) {
        raw = raw << 3; // 9 bit resolution default
        if (data[7] == 0x10) {
          // "count remain" gives full 12 bit resolution
          raw = (raw & 0xFFF0) + 12 - data[6];
        }
      } else {
        byte cfg = (data[4] & 0x60);
        // at lower res, the low bits are undefined, so let's zero them
        if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
        else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
        else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
        //// default is 12 bit resolution, 750 ms conversion time
      }
      celsius = (float)raw / 16.0;
      fahrenheit = celsius * 1.8 + 32.0;
      Serial.print("  Temperature = ");
      Serial.print(celsius);
      Serial.print(" Celsius, ");
      Serial.print(fahrenheit);
      Serial.println(" Fahrenheit");
      
      String doublequote = String("\x22");
      if ( j == 0 ) {
        //sprintf(tempstring, "%4.2f F", fahrenheit);
        dtostrf(fahrenheit, 5, 1, insidetempstringLCD);
        dtostrf(fahrenheit, 0, 1, insidetempstring);
        Serial.println(insidetempstring);
        postmesg = "POST /wurd?{";
        postmesg.concat(doublequote);
        postmesg.concat("chicken_inside");
        postmesg.concat(doublequote);
        postmesg.concat(":{");
        postmesg.concat(doublequote);
        postmesg.concat("temperature");
        postmesg.concat(doublequote);
        postmesg.concat(":");
        postmesg.concat(doublequote);
        postmesg.concat(insidetempstring);
        postmesg.concat("\"}");
      } else {
        dtostrf(fahrenheit, 5, 1, outsidetempstringLCD);
        dtostrf(fahrenheit, 0, 1, outsidetempstring);
        Serial.println(outsidetempstring);
        postmesg.concat(",\"chicken_outside\":{\"temperature\":\"");
        postmesg.concat(outsidetempstring);
        postmesg.concat("\"}} HTTP/1.0");
      }
      
      
      j++;
    }
    Serial.println();
    Serial.println("Done");
    ds.reset_search();
    
    Serial.print(now.year(), DEC);
    Serial.print('/');
    if(now.month() < 10) Serial.print("0");
    Serial.print(now.month(), DEC);
    Serial.print('/');
    if(now.day() < 10) Serial.print("0");
    Serial.print(now.day(), DEC);
    Serial.print(' ');
    if(now.hour() < 10) Serial.print("0");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    if(now.minute() < 10) Serial.print("0");
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    if(now.second() < 10) Serial.print("0");
    Serial.print(now.second(), DEC);
    Serial.println();

//    Serial.print(now.tempC(), 1);
//    Serial.println(" degrees Celcius");
//    Serial.print(now.tempF(), DEC);
//    Serial.println(" degrees Farenheit");
    
    Serial.println();
    
    LCD.write(254);  // cls
    LCD.write(1);
    LCD.write(254); // cursor to beginning of first line
    LCD.write(128);
    // outside arrow
    LCD.write(127);
    LCD.write(outsidetempstringLCD);
    LCD.write(223);
    LCD.write("F");
    // move to line 2 on LCD
    LCD.write(254);
    LCD.write(192);
    // inside arrow
    LCD.write(126);
    LCD.write(insidetempstringLCD);
    LCD.write(223);
    LCD.write("F");
    
    Serial.println("Here's what we'll send to rhogog");
    Serial.println(postmesg);
    
    if (wifly.isConnected()) {
      Serial.println("Old connection active. Closing");
      wifly.close();
    }
    // now let's post the numbers to wurd
    if (wifly.open(cthulhu, 80)) {
        Serial.print("Connected to ");
        Serial.println(cthulhu);

        /* Send the request */
        wifly.println(postmesg);
        wifly.println();
    } else {
        Serial.println("Failed to connect");
    }
    wifly.close();
    wifly.sleep(240);
    delay(40000);
  }
  delay(20000);
}
