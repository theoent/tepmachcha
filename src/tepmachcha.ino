
#define VERSION "1.03"      //  Version number

//  Customize these items for each installation
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
#define TOKEN_ID      "EWS_TOKEN"
#define DEVICE_ID     "EWS_DEVICE"

#define FTPSERVER "YOUR FTP SERVER"
#define FTPUSER   "YOUR FTP USER"
#define FTPPW     "YOUR FTP PASSWORD"

#define FTPSERVER "hackerspacephnompenh.com"
#define FTPUSER   "ftpuser"
#define FTPPW     "t0ult0mp0ng"
				                            //  to start a flow
#define SENSOR_HEIGHT  100  //  Height of top of octagonal gasket from streambed, in cm
#define UTCOFFSET        0  //  Local standard time variance from UTC
#define XBEEWINDOWSTART 14  //  Hour to turn on XBee for programming window
#define XBEEWINDOWEND   17  //  Hour to turn off XBee
#define INTERVAL        15  //  Number of minutes between readings

// jack check if we can use PSTR() here
#define BEEPASSWORD "XBEE_PASSWORD"          //  Password to turn on XBee by SMS


// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

#include <DS1337.h>           //  For the Stalker's real-time clock (RTC)
#include <Sleep_n0m1.h>       //  Sleep library
#include <SoftwareSerial.h>   //  For serial communication with FONA
#include <Wire.h>             //  I2C library for communication with RTC
//#include <I2C.h>             //  replacemnet I2C library
#include <Fat16.h>            //  FAT16 library for SD card
#include <EEPROM.h>           //  EEPROM lib
#include "Adafruit_FONA.h"

#define SERIAL_BUFFER_SIZE 16 // hardware serial receive buffer

#define RTCINT 0    //  RTC interrupt number

#define RTCPIN   2  //  Onboard Stalker RTC pin
#define FONA_RST 3  //  FONA RST pin
#define SD_POWER 4  //  optional power to SD card
#define BEEPIN   5  //  XBee power pin
#define FONA_RX  6  //  UART pin into FONA
#define PING     7  //  Sonar ping pin
#define RANGE    8  //  Sonar range pin -- pull low to turn off sonar
#define FONA_TX  9  //  UART pin from FONA

#define FONA_RTS A1 //  FONA RTS pin
#define FONA_KEY A2 //  FONA Key pin
#define FONA_PS  A3 //  FONA power status pin
#define BATT     A7 //  Battery level


#define DOTIMES(x) for(uint16_t _i=x; _i;_i--)

static const char OK_STRING[] PROGMEM = "OK";
//__FlashStringHelper* OK = (__FlashStringHelper*)OK_STRING;
#define OK ((__FlashStringHelper*)OK_STRING)

// call into bootloader jumptable at top of flash
#define write_flash_page (*((void(*)(const uint32_t address))(0x7ffa/2)))
#define flash_firmware (*((void(*)(void))(0x7ffc/2)))
#define EEPROM_FILENAME_ADDR (E2END - 1)

boolean sentData = false;
boolean smsPower = false;       //  Manual XBee power flag
boolean noSMS = false;          //  Flag to turn off SMS checking -- for future use
boolean timeReset = false;      //  Flag indicating whether midnight time reset has already occurred
byte beeShutoffHour = 0;        //  Hour to turn off manual power to XBee
byte beeShutoffMinute = 0;      //  Minute to turn off manual power to XBee
char method = 0;                //  Method of clock set, for debugging

DateTime now;

SoftwareSerial fonaSerial = SoftwareSerial (FONA_TX, FONA_RX);
Adafruit_FONA fona = Adafruit_FONA (FONA_RST);

DS1337 RTC;         //  Create the DS1337 real-time clock (RTC) object
Sleep sleep;        //  Create the sleep object


static void rtcIRQ (void)
{
		RTC.clearINTStatus();   //  Wake from sleep and clear the RTC interrupt
}


void setup (void)
{
		Wire.begin(); //  Begin the I2C interface
		RTC.begin();  //  Begin the RTC        
		Serial.begin (57600);

    // Compile-in the date and time; helps identify software uploads
		// Note: C compiler concatenates adjacent strings
		Serial.println (F("Tepmachcha v" VERSION " " __DATE__ " " __TIME__));

    // after change reference, first couple readings can be wrong
		analogReference(DEFAULT); // atmega328: DEFAULT=3.3V, INTERNAL=3.3V, EXTERNAL=3.3V
		readBattery(); delay(100);
    readBattery(); delay(100);

		Serial.print (F("Battery: "));
		//Serial.print (readBattery);
		Serial.println (F("mV"));

		pinMode (BEEPIN, OUTPUT);
		pinMode (RANGE, OUTPUT);
		pinMode (FONA_KEY, OUTPUT);
		pinMode (FONA_RX, OUTPUT);


		/*   If the voltage at startup is less than 3.5V, we assume the battery died in the field
		 *   and the unit is attempting to restart after the panel charged the battery enough to
		 *   do so. However, running the unit with a low charge is likely to just discharge the
		 *   battery again, and we will never get enough charge to resume operation. So while the
		 *   measured voltage is less than 3.5V, we will put the unit to sleep and wake once per 
		 *   hour to check the charge status.
		 */
		while (readBattery() < 3500)   
		{
				Serial.println (F("Low power sleep"));
				Serial.flush();
				digitalWrite (BEEPIN, HIGH);      //  Make sure XBee is powered off
				digitalWrite (RANGE, LOW);        //  Make sure sonar is off
				RTC.enableInterrupts (EveryHour); //  We'll wake up once an hour
				RTC.clearINTStatus();             //  Clear any outstanding interrupts
				attachInterrupt (RTCINT, rtcIRQ, FALLING);
				interrupts();
				sleep.pwrDownMode();                    //  Set sleep mode to Power Down
				sleep.sleepInterrupt (RTCINT, FALLING); //  Sleep; wake on falling voltage on RTC pin
		}

		digitalWrite (RANGE, HIGH);          //  If set low, sonar will not range
		digitalWrite (FONA_KEY, HIGH);       //  Initial state for key pin

    fonaPowerOn();
    fonaSerialOn();
    ftpGet();
    //return;

      //SP=RAMEND;
      //flash_firmware();
    // JACK
    //asm("cli");
		if (fonaOn() ) {
      Serial.println(F("jack"));
      delay(2000);

      int x = 3;
      while( x-- ) {
        Serial.print(F("try "));
        Serial.println(x);
        if (getFirmware())
          break;
      }
      fonaOff();
    }
    //while (1);
      Serial.println(F("end"));
      delay(2000);

		// We will use the FONA to get the current time to set the Stalker's RTC
		fonaOn();
		clockSet();
		
		// Delete any accumulated SMS messages to avoid interference from old commands
		fona.sendCheckReply (F("AT+FSDRIVE=0"), OK);            //  Enter text mode
		fona.sendCheckReply (F("AT+FSLS=?"), OK);            //  Enter text mode
		
		fonaOff();
		
		RTC.enableInterrupts (EveryMinute);  //  RTC will interrupt every minute
		RTC.clearINTStatus();                //  Clear any outstanding interrupts
		attachInterrupt (RTCINT, rtcIRQ, FALLING);
		interrupts();

		now = RTC.now();    //  Get the current time from the RTC
		
		//  We'll keep the XBee on for an hour after startup to assist installation
		if (now.hour() == 23)
		{
				beeShutoffHour = 0;
		}
		else
		{
				beeShutoffHour = (now.hour() + 1);
		}
		beeShutoffMinute = now.minute();

		Serial.print (F("XBee powered until"));
		Serial.print (beeShutoffHour);
		Serial.print (F(":"));
		Serial.println (beeShutoffMinute);
		Serial.flush();
		smsPower = true;
}


void loop (void)
{

		now = RTC.now();      //  Get the current time from the RTC

		Serial.print (now.hour());
		Serial.print (F(":"));
		Serial.println (now.minute());

		int streamHeight = takeReading();
    //return;         // JACK

		/*  One failure mode of the sonar -- if, for example, it is not getting enough power -- 
	   *	is to return the minimum distance the sonar can detect; in the case of the 10m sonars
	   *	this is 50cm. This is also what would happen if something were to block the unit -- a
	   *	plastic bag that blew onto the enclosure, for example. We very much want to avoid false
	   *	positive alerts, so for the purposes of yellow and red alerts, we will ignore anything
	   *	less than 55cm from the sensor. 
     *
	   *	Per discussions with PIN, alerts will be cleared manually, by sending an SMS to the 
	   *	unit.
     */

		if (now.minute() % INTERVAL == 0 && sentData == false)   //  If it is time to send a scheduled reading...
		{
				upload (streamHeight);
		}
		else
		{
				sentData = false;
		}

		//  We will turn on the XBee radio for programming only within a specific
		//  window to save power
		if (now.hour() >= XBEEWINDOWSTART && now.hour() <= XBEEWINDOWEND)
		{
				digitalWrite (BEEPIN, LOW);
		}
		else
		{
				//  If the XBee power was turned on by SMS, we'll check to see if 
				//  it's time to turn it back off
				if(smsPower == true && now.hour() < beeShutoffHour)
				{
				        digitalWrite (BEEPIN, LOW);
				}
				else
				{
				        if(smsPower == true && now.hour() == beeShutoffHour && now.minute() < beeShutoffMinute)
				        {
				                digitalWrite (BEEPIN, LOW);
				        }
				        else
				        {
				                if (smsPower == true && now.hour() == 23 && beeShutoffHour == 0)
				                {
				                        digitalWrite (BEEPIN, LOW);
				                }
				                else
				                {
				                        if (smsPower == true)
				                        {
				                                  Serial.println (F("Turning XBee off..."));
				                                  Serial.flush();
				                                  wait (500);
				                        }
		
				                        digitalWrite (BEEPIN, HIGH);
				                        smsPower = false;
				                }
				        }
				}
		}

		Serial.flush();                         //  Flush any output before sleep

		sleep.pwrDownMode();                    //  Set sleep mode to "Power Down"
		RTC.clearINTStatus();                   //  Clear any outstanding RTC interrupts
		sleep.sleepInterrupt (RTCINT, FALLING); //  Sleep; wake on falling voltage on RTC pin
}


void upload(int streamHeight)
{
		fonaOn();
		sendReading (streamHeight);

		if (noSMS == false)
		{
				checkSMS();
		}

		/*   The RTC drifts more than the datasheet says, so we'll reset the time every day at
		 *   midnight. 
		 *
     */
		if (now.hour() == 0 && timeReset == false)
		{
				clockSet();
				timeReset = true;
		}

		if (now.hour() != 0)
		{
				timeReset = false;
		}
		
		fonaOff();

		sentData = true;
}



void clockSet (void)
{
		wait (1000);    //  Give time for any trailing data to come in from FONA

		int netOffset;

		char theDate[17];

		Serial.println (F("Fetching GSM time"));

		fonaFlush();    //  Flush any trailing data

		fona.sendCheckReply (F("AT+CIPGSMLOC=2,1"), OK);    //  Query GSM location service for time
		
		fona.parseInt();                    //  Ignore first int
		int secondInt = fona.parseInt();    //  Ignore second int -- necessary on some networks/towers
		int netYear = fona.parseInt();      //  Get the results -- GSMLOC year is 4-digit
		int netMonth = fona.parseInt();
		int netDay = fona.parseInt();
		int netHour = fona.parseInt();      //  GSMLOC is _supposed_ to get UTC; we will adjust
		int netMinute = fona.parseInt();
		int netSecond = fona.parseInt();    //  Our seconds may lag slightly, of course

		if (netYear < 2016 || netYear > 2050 || netHour > 23) //  If that obviously didn't work...
		{
				netSecond = netMinute;  //  ...shift everything up one to capture that second int
				netMinute = netHour;
				netHour = netDay;
				netDay = netMonth;
				netMonth = netYear;
				netYear = secondInt;

				Serial.println (F("Recombobulating..."));
		}

		if (netYear < 2016 || netYear > 2050 || netHour > 23)   // If that still didn't work...
		{
				Serial.println (F("GSM location service failed"));
				/*   ...the we'll get time from the NTP pool instead: 
				 *   (https://en.wikipedia.org/wiki/Network_Time_Protocol)
				 */
				fona.enableNTPTimeSync (true, F("0.daimakerlab.pool.ntp.org"));
				Serial.println (F("enabling NTP sync"));
				
				wait (15000);                 // Wait for NTP server response
				
				fona.println (F("AT+CCLK?")); // Query FONA's clock for resulting NTP time              
				netYear = fona.parseInt();    // Capture the results
				netMonth = fona.parseInt();
				netDay = fona.parseInt();
				netHour = fona.parseInt();    // We asked NTP for UTC and will adjust below
				netMinute = fona.parseInt();
				netSecond = fona.parseInt();  // Our seconds may lag slightly
		
				method = 'N';
		}
		else 
		{
				method = 'G';
		}

		if ((netYear < 1000 && netYear >= 16 && netYear < 50) || (netYear > 1000 && netYear >= 2016 && netYear < 2050))
		//  If we got something that looks like a valid date...
		{
				//  Adjust UTC to local time
				if((netHour + UTCOFFSET) < 0)                   //  If our offset + the UTC hour < 0...
				{
				        netHour = (24 + netHour + UTCOFFSET);   //  ...add 24...
				        netDay = (netDay - 1);                  //  ...and adjust the date to UTC - 1
				}
				else
				{
				        if((netHour + UTCOFFSET) > 23)          //  If our offset + the UTC hour > 23...
				        {
				                netHour = (netHour + UTCOFFSET - 24); //  ...subtract 24...
				                netDay = (netDay + 1);                //  ...and adjust the date to UTC + 1
				        }
				        else
				        {
				                netHour = (netHour + UTCOFFSET);      //  Otherwise it's straight addition
				        }
				}

				Serial.print (F("Obtained current time: "));
        /*
				sprintf (theDate, "%d/%d/%d %d:%d", netDay, netMonth, netYear, netHour, netMinute);
				Serial.print (theDate);
        */
				Serial.print (netDay);
        Serial.print ('/');
				Serial.print (netMonth);
        Serial.print ('/');
				Serial.print (netYear);
        Serial.print (' ');
				Serial.print (netHour);
        Serial.print (':');
				Serial.println (netMinute);

				Serial.println(F("Adjusting RTC"));
				DateTime dt(netYear, netMonth, netDay, netHour, netMinute, netSecond, 0);
				RTC.adjust(dt);     //  Adjust date-time as defined above
		}
		else
		{
				Serial.println (F("Network time failed, continue with RTC"));
				method = 'X';
		}

		wait (200);              //  Give FONA a moment to catch its breath
}


void wait (unsigned long period)
{
		//  Non-blocking delay function
		unsigned long waitend = millis() + period;
		while (millis() <= waitend)
		{
				Serial.flush();
		}
}


void fonaFlush (void)
{
    // Read all available serial input from FONA to flush any pending data.
		while(fona.available())
		{
				char c = fona.read();
				Serial.print (c);
		}
}

char fonaRead(void)
{
  // read from fona with timeout
  uint32_t timeout = millis() + 1000;

  while(!fona.available()) {
    if (millis() > timeout)
      break;
    delay(1);
  }
  return fona.read();
}


void fonaPowerOn(void)
{
  if (digitalRead (FONA_PS) == LOW)             //  If the FONA is off...
  {
      digitalWrite(FONA_KEY, HIGH);  //  KEY should be high to start

      Serial.print (F("FONA poweron"));
      while (digitalRead (FONA_PS) == LOW) 
      {
              digitalWrite(FONA_KEY, LOW);  //  ...pulse the Key pin low...
              wait (500);
      }
      digitalWrite (FONA_KEY, HIGH);        //  ...and then return it to high
      Serial.println(F(" done."));
  }
}


boolean fonaSerialOn(void)
{
  Serial.println (F("Initializing FONA"));
  
  fonaSerial.begin (4800);                      //  Open a serial interface to FONA

  if (fona.begin (fonaSerial) == false)         //  Start the FONA on serial interface
  {
      Serial.println (F("FONA not found"));
      return false;
  }
  return true;
}


boolean fonaGSMOn(void) {
				unsigned long gsmTimeout = millis() + 30000;
				boolean gsmTimedOut = false;

				Serial.print (F("Connecting GSM... "));
				while (true)
				{
				        byte network_status = fona.getNetworkStatus();
				        if(network_status == 1 || network_status == 5) break;
				        
				        if(millis() >= gsmTimeout)
				        {
				                gsmTimedOut = true;
				                break;
				        }
				        wait (500);
				}

				if(gsmTimedOut == true)
				{
				        Serial.println (F("timed out. Check SIM card, antenna, and signal."));
				        return false;
				}
				else
				{
				        Serial.println(F("done."));
                return true;
				}

}


void fonaGPRSOn(void) {
}


boolean fonaOn()
{
    fonaPowerOn();
    if (fonaSerialOn()) {
				Serial.print (F("FONA online. "));
        if ( fonaGSMOn() ) {

          //  RSSI is a measure of signal strength -- higher is better; less than 10 is worrying
          byte rssi = fona.getRSSI();
          Serial.print (F("RSSI: "));
          Serial.println (rssi);

          wait (3000);    //  Give the network a moment

          fona.setGPRSNetworkSettings (F("cellcard"));    //  Set APN to your local carrier

          if (rssi > 5)
          {
                  if (fona.enableGPRS (true) == false);
                  {
                        //  Sometimes enableGPRS() returns false even though it succeeded
                        if (fona.GPRSstate() != 1)
                        {
                                for (byte GPRSattempts = 0; GPRSattempts < 10; GPRSattempts++)
                                {
                                      Serial.println (F("Trying again..."));
                                      wait (2000);
                                      fona.enableGPRS (true);
                                      
                                      if (fona.GPRSstate() == 1)
                                      {
                                              Serial.println (F("GPRS is on."));
                                              break;
                                      }
                                      else
                                      {
                                              Serial.print (F("Failed to turn GPRS on... "));
                                      }
                                }
                        }
                  }
          }
          else
          {
                  Serial.println (F("Inadequate signal strength"));
                  //gsmTimedOut = true;
          }
          
          return true;
      }
    }
}



void fonaOff (void)
{
		wait (5000);        //  Shorter delays yield unpredictable results

		//  We'll turn GPRS off first, just to have an orderly shutdown
		if (fona.enableGPRS (false) == false)
		{
				if (fona.GPRSstate() == 1)
				{
				        Serial.println (F("Failed to turn GPRS off."));
				}
				else
				{
				        Serial.println (F("GPRS is off."));
				}
		}
	  
		wait (500);
	  
		// Power down the FONA if it needs it
		if (digitalRead (FONA_PS) == HIGH)      //  If the FONA is on...
		{
				fona.sendCheckReply (F("AT+CPOWD=1"), OK); //  ...send shutdown command...
				digitalWrite (FONA_KEY, HIGH);                  //  ...and set Key high
		}
}


int takeReading (void)
{
		//  We will take the mode of seven samples to try to filter spurious readings
		int sample[] = {0, 0, 0, 0, 0, 0, 0};   //  Initial sample values
		
		for (int sampleCount = 0; sampleCount < 7; sampleCount++)
		{
				sample[sampleCount] = pulseIn (PING, HIGH);
				Serial.print (F("Sample "));
				Serial.print (sampleCount);
				Serial.print (F(": "));
				Serial.println (sample[sampleCount]);
				wait (10);
		}

		int sampleMode = mode (sample, 7);

		int streamHeight = (SENSOR_HEIGHT - (sampleMode / 10)); //  1 µs pulse = 1mm distance

		Serial.print (F("Surface distance from sensor is "));
		Serial.print (sampleMode);
		Serial.println (F("mm."));
		Serial.print (F("Calculated surface height is "));
		Serial.print (streamHeight);
		Serial.println (F("cm."));

		return streamHeight;
}


boolean validate (int alertThreshold)
{
  /*   False positives would undermine confidence in the IVR alerts, so we must take
   *   pains to avoid them. Before triggering an Ilhapush alert flow, we will validate
   *   the reading by taking five readings and making sure they _all_ agree. If the
   *   levels are marginal, that might mean we don't send an alert for a while (because
   *   some readings might come in below the threshold).
   */

  boolean valid = true;

  for (int i = 0; i < 5; i++)
  {
	  wait (5000);
	  Serial.print (F("Validation reading #"));
	  Serial.println (i);

	  int doubleCheck = takeReading();

	  if (doubleCheck < alertThreshold)
    {
	    Serial.println (F("Validation does not agree."));
	    valid = false;
	    break;
	  }
  }

  return valid;
}


int mode (int *x, int n)    
/*   Calculate the mode of an array of readings
 *   From http://playground.arduino.cc/Main/MaxSonar
 */   
{
		int i = 0;
		int count = 0;
		int maxCount = 0;
		int mode = 0;
	  
		int bimodal;
		int prevCount = 0;
		while(i < (n - 1))
		{
				prevCount = count;
				count = 0;
				while(x[i] == x[i + 1])
				{
				        count++;
				        i++;
				}

				if(count > prevCount && count > maxCount)
				{
				        mode = x[i];
				        maxCount = count;
				        bimodal = 0;
				}
				
				if(count == 0)
				{
				        i++;
				}
				
				if(count == maxCount)     //  If the dataset has 2 or more modes
				{
				        bimodal = 1;
				}
				
				if(mode == 0 || bimodal == 1)   //  Return the median if no mode
				{
				        mode = x[(n / 2)];
				}
				
				return mode;
		}
}

boolean sendReading (int streamHeight)
{
        uint16_t statuscode = 0;
        int16_t length = 0;
        char data[255];

        unsigned int voltage;
        fona.getBattVoltage (&voltage);   //  Read the battery voltage from FONA's ADC

        int solar;
        if (analogRead (6) <= 900 && analogRead (6) > 550)  //  Determine if panel is charging
        {
                solar = 1;
        }
        else
        {
                solar = 0;
        }

        //  Construct the body of the POST request:
        sprintf (data, "api_token=%s&data={\"sensorId\":\"%s\",\"streamHeight\":\"%d\",\"charging\":\"%d\",\"voltage\":\"%d\",\"timestamp\":\"%d-%d-%dT%d:%d:%d.000Z\"}\r\n", TOKEN_ID, DEVICE_ID, streamHeight, solar, voltage, now.year(), now.month(), now.date(), now.hour(), now.minute(), now.second());

        Serial.println (F("Sending data:"));
        Serial.println (data);

        //  ews1294.info does not currently support SSL; if it is added you will need to uncomment the following
        //fona.sendCheckReply (F("AT+HTTPSSL=1"), F("OK"));   //  Turn on SSL
        //fona.sendCheckReply (F("AT+HTTPPARA=\"REDIR\",\"1\""), F("OK"));  //  Turn on redirects (for SSL)

        //  Send the POST request we have constructed
        fona.HTTP_POST_start ("ews1294.info/api/v1/sensorapi", F("application/x-www-form-urlencoded"), (uint8_t *)data, strlen (data), &statuscode, (uint16_t *) &length);
        fona.println (F("AT+HTTPREAD"));
        
        while (length > 0)
        {
                fonaFlush();

                length--;
                if (!length)
                {
                        break;
                }
        }

        if (statuscode == 200)
        {
                Serial.println (F("POST request succeeded."));
                return true;
        }
        else
        {
                Serial.print (F("POST request appeared to fail. Status-code: "));
                Serial.println (statuscode);
                return false;
        }

        fona.HTTP_POST_end();
}


void checkSMS (void)
{
		//  Check SMS messages received for any valid commands
		
		char smsBuffer[255];  //??
		char smsSender[20];
		char smsMsg[57];

		unsigned int smsLen;
		boolean sendStatus = false;
		int NumSMS;

		fonaFlush();    //  Flush out any unresolved data

		/*   During testing on metfone, sometimes the FONA had to be on for more
		 *   than 60 seconds(!) before it would receive a previously sent SMS message,
		 *   so we'll keep looking for 60 seconds.
		 */

		unsigned long smsTimeout = millis() + 60000;

		Serial.println (F("Checking for SMS messages..."));

		do
		{
				NumSMS = fona.getNumSMS();
				wait (5000);
		}
		while (NumSMS == 0 && millis() <= smsTimeout);
		
		Serial.print (NumSMS);
		Serial.println (F(" message(s) waiting."));

		unsigned long timeOut = (millis() + 60000);
		
		while (NumSMS > 0)          //  If there are messages...
		{
				fona.readSMS (NumSMS, smsBuffer, 250, &smsLen);  //  ...retrieve the last one...
				
				wait (500);                                      //  ...give FONA a moment to catch up...
				
				fona.getSMSSender (NumSMS, smsSender, 250);      //  ...find out who send it...
				
				wait (500);

				Serial.print (F("Message from "));
				Serial.print (smsSender);
				Serial.println (F(":"));
				Serial.println (smsBuffer);

				//  Now check to see if any of the declared passwords are in the SMS message and respond accordingly
				if (strcmp_P (smsBuffer, (const char *)F(BEEPASSWORD)) == 0)        //  If the message is the XBee password...
				{
				        //  ...determine the appropriate shutoff time and turn on the XBee until then
				        
				        if (now.hour() == 23)
				        {
				                beeShutoffHour = 0;
				        }
				        else
				        {
				                beeShutoffHour = now.hour() + 1;    //  We'll leave the XBee on for 1 hour
				        }
				        
				        beeShutoffMinute = now.minute();
				        
				        digitalWrite (BEEPIN, LOW);        //  Turn on the XBee

                /*
                char leadingZero[2];
				        if (beeShutoffMinute < 10)         //  Add a leading zero to the minute if necessary
				        {
                        leadingZero[0] = '0';
				        }

				        //  Compose a reply to the sender confirming the action and giving the shutoff time
				        sprintf (smsMsg, "XBee on until %d:%s%d", beeShutoffHour, leadingZero, beeShutoffMinute);
                */

				        // Compose a reply to the sender confirming the action and giving the shutoff time
		            strcat_P(smsBuffer, (const char *)F("XBee on until %d:%02d"));
				        sprintf (smsMsg, smsBuffer, beeShutoffHour, beeShutoffMinute);

				        Serial.println (F("XBee turned on by SMS."));
				        smsPower = true;                    //  Raise the flag 
				        fona.sendSMS(smsSender, smsMsg);    //  Tell the sender what you've done
				}


				wait (1000);
				fona.deleteSMS (NumSMS);
				wait (1500);
				NumSMS = fona.getNumSMS();

				//  Occasionally messages won't delete and this loops forever. If
				//  the process takes too long we'll just nuke everything.
				if (millis() >= timeOut)
				{
				        fona.sendCheckReply (F("AT+CMGF=1"), OK);            //  Enter text mode
				        fona.sendCheckReply (F("AT+CMGDA=\"DEL ALL\""), OK); //  Delete all SMS messages                }
				}
		}
}



/* Get battery reading on ADC pin BATT, in mV
 *
 * VBAT is divided by a 10k/2k voltage divider (ie /6) to BATT, which is then
 * measured relative to AREF, on a scale of 0-1023, so
 *
 *   mV = BATT * (AREF * ( (10+2)/2 ) / 1.023)
 *
 * We use integer math to avoid including ~1.2K of FP/multiplication library
 * AREF ADC->mV factor   approx integer fraction
 * ---- --------------   -----------------------
 * 1.1  6.4516           1651/256 (~103/16)
 * 3.3  19.3548          4955/256 (~155/8)
 *
 * Note, if we ONLY needed to compare the ADC reading to a FIXED voltage,
 * we'd simplify by letting the compiler calculate the equivalent 0-1023 value
 * at compile time, and compare against that directly.
 * eg to check voltage is < 3500:
 *   if (analogueRead(BATT) < 3500/19.3548) {...}
 *
 * Also the calculations don't really need to be too accurate, because the
 * ADC itself can be be quite innacurate (up to 10% with internal AREF!)
 */
uint16_t readBattery(void) {

  uint16_t adc;
  uint32_t mV;
  
  adc = analogRead(BATT);

  // We could just add 155 times, but it's quicker to add/substract
  // powers-of-two multiples of adc, which can be easily be calculated
  // by shifting bits, eg shift <<2 bits for *4
  // 155 = 128 + 32 - 4 - 1
  mV = adc * 128;  // compiler smarts should convert * to equivalent <<
  mV += adc * 32;
  mV -= adc * 4;
  mV -= adc;
  return mV / 8;

  /* values for 1.1v analogue reference AREF
  // 103 = 64 + 32 + 8 - 1
  mV = adc * 64;
  mV += adc * 32;
  mV += adc * 8;
  mV -= adc;
  return mV / 16;
  */
}


//#define CHIP_SELECT SS;  // SD chip select pin. (SS = 10)
const uint8_t CHIP_SELECT = SS;  // SD chip select pin (SS = 10)
SdCard card;
Fat16 file;
const char file_name[13] = "TEP.BIN";
//const char file_name[] = "FIRMWARE.HEX";
//const char file_name[] = "BLINK.HEX";

boolean fat_init(void) {

  // init sd card
  if (!card.begin(CHIP_SELECT)) {
    Serial.print(F("failed card.begin ")); Serial.println(card.errorCode);
	  return false;
  }
  
  // initialize a FAT16 volume
  if (!Fat16::init(&card)) {
	  Serial.println(F("Fat16::init failed"));
	  return false;
  }

  file.open(file_name, O_CREAT | O_WRITE | O_TRUNC);
  Serial.print(F("file: ")); Serial.println(file_name);
  return true;
}


// write firmware filename to EEPROM and toggle boot-from-SDcard flag at EEPROM[E2END]
void writeEeprom(void)
{
  uint8_t x;

    //Serial.print('|');
  for (x = 0; x < 12 && file_name[x] != 0; x++) {
    //Serial.print(file_name[x]);
    //Serial.print(',');
    EEPROM.write( (EEPROM_FILENAME_ADDR - x), file_name[x] );
  }
    //Serial.println('|');

  EEPROM.write(E2END, 0); // 0 triggers an attempt to flash from SD card on power-on or reset
}

// serial flow control on
#define XON  17
void xon(void) {
  //fonaSerial.write(XOFF);
  digitalWrite(FONA_RTS, HIGH);
}

// serial flow control off
#define XOFF 19
void xoff(void) {
  //fonaSerial.write(XON);
  digitalWrite(FONA_RTS, LOW);
}


/*
// HTTP file download - working for small files
uint8_t httpGet(void)
{
  char url[128];
  unsigned int httpStatus = 0;
  unsigned int datalen = 0;
  uint8_t tries;

  fona.sendCheckReply (F("AT+HTTPINIT"), OK);
  fona.sendCheckReply (F("AT+HTTPPARA=\"CID\",1"), OK);
  //fona.sendCheckReply (F("AT+HTTPSSL=1"), OK);
  fona.sendCheckReply (F("AT+HTTPPARA=\"REDIR\",\"1\""), OK);
  fona.sendCheckReply (F("AT+HTTPPARA=\"UA\",\"FONA\""), OK);

  //fona.sendCheckReply (F("AT+HTTPPARA=\"URL\",\"csb.stanford.edu/class/public/pages/sykes_webdesign/05_simple.html\""), OK);
  fona.sendCheckReply (F("AT+HTTPPARA=\"URL\",\"jackbyte.fastmail.fm/pin/blink.hex\""), OK);

  for (tries = 5; tries > 0; tries--) {
    fona.HTTP_action(0, &httpStatus, &datalen, 30000);

    Serial.print("status ");
    Serial.println(httpStatus);
    Serial.print("size ");
    Serial.println(datalen);
    if (httpStatus == 200) break;
    delay(2000);
  }

  if (httpStatus == 200) {   //  If the HTTP GET request returned a 200, it succeeded
    fona.HTTP_readall(datalen);
  }
  //fona.HTTP_GET_end();
}
*/


// Read len bytes into buffer w/ timeout
uint16_t readBuffer(char *buf, uint16_t len)
{
  uint16_t n = 0;
  uint32_t timeout = millis() + 1000; // shouldn't take more than a second

  xon();
  while (len > 0 && millis() < timeout) {
    if (fona.available()) {
      buf[n++] = fona.read();
      len--;
    }
  }
  xoff();

  return n;
}


#define READ_BUFFER_SIZE 128
boolean ftpCopyFile(uint16_t len)
{
  uint32_t address = 0;
  uint32_t crc = 0;
  //char buf[READ_BUFFER_SIZE];
char buf[READ_BUFFER_SIZE+1]; // extra needed only for println
  uint16_t size;
  uint16_t n;

  size = READ_BUFFER_SIZE;

  // quiet mode - commands don't return status
  fona.sendCheckReply (F("ATQ1"), F(""));
  fonaFlush();

  while (address < len) {

    uint8_t block_retries = 2;

    // SPR sprintf (buf, "AT+FSREAD=C:\\User\\ftp\\tmp.bin,1,%d,%ld", size, address);
    fonaSerial.print(F("AT+FSREAD=C:\\User\\ftp\\tmp.bin,1,"));
    fonaSerial.print(size);
    fonaSerial.print(',');
    fonaSerial.println(address);

//Serial.println(buf);

    delay(50);
    fona.read();
    fona.read();

    //fonaRead();
    //fonaRead();
    /*
    // Flushfona returns \r\n from submitting command despite ATQ1
    if (fonaRead() != '\r') break;  // eat \r
    if (fonaRead() != '\n') break;  // eat \n
      */

    n = readBuffer(buf, size);

    if (n == size) {
      file.write(buf, n);
    } else {

Serial.print(F("only: "));
Serial.println(n);

      if (block_retries--) {
        continue;
      } else {
        break;
      }
    }

Serial.print(F("wrote:"));
Serial.println(n);
Serial.println(F("==============="));

    // catch any stray OK responses etc
    fonaFlush();

/*
Serial.println(F("---------------"));
buf[size+1] = 0;
Serial.print(buf);
Serial.println();
Serial.println(F("---------------"));
*/
//Serial.print(F("."));

    address += size;
    if (address + size > len)  // FIXME probably should by >= ??
      size = len - address;    // FIXME len-address, or len-address-1 ??
  }

  //rewind
  //bool seekCur(int32_t pos) {return seekSet(curPosition_ + pos);}

  return true;
}


void ftpEnd(void)
{
  fona.sendCheckReply (F("AT+FTPQUIT"), OK);
  fona.sendCheckReply (F("AT+FTPSHUT"), OK);
}


/*
 * Fetch firmware from FTP server to FONA's internal filesystem
 */
boolean ftpGet(void)
{
  char buf[32];
  boolean success;

  // FTP download to file supported and ready?
  // JACK if (!fona.sendCheckReply (F("AT+FTPGETTOFS?"), F("+FTPGETTOFS: 0"))) return false;

  // configure FTP
  fona.sendCheckReply (F("AT+SSLOPT=0,1"), OK); // 0,x dont check cert, 1,x client auth
  fona.sendCheckReply (F("AT+FTPSSL=0"), OK);   // 0 ftp, 1 implicit (port is an FTPS port), 2 explicit
  fona.sendCheckReply (F("AT+FTPCID=1"), OK);
  fona.sendCheckReply (F("AT+FTPMODE=1"), OK);     // 0 ACTIVE, 1 PASV
  fona.sendCheckReply (F("AT+FTPTYPE=\"I\""), OK); // "I" binary, "A" ascii
  fona.sendCheckReply (F("AT+FTPSERV=\"" FTPSERVER "\""), OK);
  fona.sendCheckReply (F("AT+FTPUN=\"" FTPUSER "\""), OK);
  fona.sendCheckReply (F("AT+FTPPW=\"" FTPPW "\""), OK);

  // remote filename
  
  /*
  strcat_P(buf, p_ftpgetname);
  strcat(buf, file_name);
  strcat_P(buf, p_quote);
  */
  // SPR sprintf(buf, p_ftpget_format, file_name);
  //fona.sendCheckReply (buf, OK);
  //fona.sendCheckReply (buf, OK);

  Serial.print(F("AT+FTPGETNAME=\""));
  Serial.print(file_name);
  Serial.println(F("\""));

  fonaSerial.print(F("AT+FTPGETNAME=\""));
  fonaSerial.print(file_name);
  fonaSerial.println(F("\""));
  fona.expectReply (OK);

  // local file path on fona
  fona.sendCheckReply (F("AT+FSDEL=C:\\User\\ftp\\tmp.bin"), OK); // delete previous download file
  fona.sendCheckReply (F("AT+FTPGETPATH=\"/home/ftpuser/files/\""), OK);

  // start the download to local file
  fona.sendCheckReply (F("AT+FTPGETTOFS=0,\"tmp.bin\""), OK);

  // Wait for download complete; FTPGETOFS status 0
  uint32_t timeout = millis() + 90000;
  while( !fona.sendCheckReply (F("AT+FTPGETTOFS?"), F("+FTPGETTOFS: 0")) ) {
    delay(2000);
    if (millis() > timeout) {
      return false;
    }
  }

  // Check the file exists
  if (fona.sendCheckReply (F("AT+FSFLSIZE=C:\\User\\ftp\\tmp.bin"), F("ERROR"))) {
    return false;
  }

  return true;
}



boolean getFirmware()
{ 
  if ( !(fat_init() && file.isOpen()) ) {
    return false;
  }
  Serial.println(F("here"));
  delay(1000);

  if (fona.GPRSstate() != 1) {
    Serial.println(F("no gprs"));
    delay(1000);
    //return false;
  }

  Serial.println(F("Fetching firmware"));
  delay(1000);

  //fona.sendCheckReply (F("AT+IFC=?"), OK);
  //fona.sendCheckReply (F("AT+IFC=2,2"), OK);     // set RTS

  /*
  fona.sendCheckReply (F("AT+FSDRIVE=0"), OK);
  fona.sendCheckReply (F("AT+FSMEM"), OK);
  fona.sendCheckReply (F("AT+FSLS=C:\\User\\ftp\\"), OK);
  fona.sendCheckReply (F("AT+FSFLSIZE=C:\\User\\ftp\\tmp.bin"), OK);
  */

  if ( ftpGet() ) {
    ftpEnd();

    if (ftpCopyFile(23258)) {

      delay(1000);
      file.close();

      delay(1000);
      fonaOff();

      Serial.println(F("updating eeprom...."));
      delay(100);
      writeEeprom();
      Serial.println(F("reflashing...."));
      delay(100);

      SP=RAMEND;
      flash_firmware();

    }
  }

  return false;
  //fonaOff();
}
