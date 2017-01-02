#include "LogLib.h"

//int chipSelect = 4; //Arduino Mega
//int chipSelect = 6; //TeensyGPS version 1.0
int chipSelect = 15; //TeensyGPS version 1.1
int mosi = 7;
int miso = 8;
int sck = 14;

//Teensy pins
#define ADAFRUITBLE_CS  21      //TeensyGPS version 1.1
#define ADAFRUITBLE_REQ 21
#define ADAFRUITBLE_RDY 24     // This should be an interrupt pin, on Uno thats #2 or #3
#define ADAFRUITBLE_RST 25
Adafruit_BLE_UART BTLEserial = Adafruit_BLE_UART(ADAFRUITBLE_REQ, ADAFRUITBLE_RDY, ADAFRUITBLE_RST);

Sd2Card card;
File dataFile;

boolean sd_datalog = 1;
boolean log_output;
boolean parse_success;
boolean file_open = false;
boolean file_log = true;
boolean config_open = true;
int count;
int checksums = 0;
int fileNum = 0; 
char namefile[13]="LOG00001.CSV";
char nameConfig[12]="config.jsn";

StaticJsonBuffer<700> jsonBuffer1;
StaticJsonBuffer<700> jsonBuffer2;
StaticJsonBuffer<700> jsonBuffer3;
StaticJsonBuffer<700> jsonBuffer4;
StaticJsonBuffer<700> jsonBuffer5;
StaticJsonBuffer<700> jsonBuffer6;
const char* classConfig[number_JSON_object];

char LineOut_name[3][10] = {"Disabled","High","Low"};

float CNF [15];
boolean TPV [19];
boolean ATT [24];

String CNF_name[15] = {"log_en", 
                       "canspeed", 
                       "newlog", 
                       "rate", 
                       "size", 
                       "blat", 
                       "blong",
                       "btime", 
                       "btol", 
                       "log_type", 
                       "trig", 
                       "trigv", 
                       "intv", 
                       "min", 
                       "max"};
String TPV_name[19] = {"device", 
                       "mode", 
                       "sv",
                       "time", 
                       "lat", 
                       "lon", 
                       "alt", 
                       "latf", 
                       "lonf", 
                       "altf", 
                       "track", 
                       "speed", 
                       "speedf", 
                       "gdop", 
                       "pdop", 
                       "hdop", 
                       "vdop", 
                       "tdop",
                       "error"};
String ATT_name[24] = {"device", 
                       "time", 
                       "heading", 
                       "pitch", 
                       "yaw", 
                       "roll",
                       "dip", 
                       "mag_len", 
                       "mag_x", 
                       "mag_y", 
                       "mag_z", 
                       "acc_len", 
                       "acc_x", 
                       "acc_y", 
                       "acc_z", 
                       "gyro_x", 
                       "gyro_y",
                       "gyro_z",
                       "quat1",
                       "quat2",
                       "quat3",
                       "quat4",
                       "temp",
                       "pressure"};
                       
String CAN_name[NUM_CAN_FRAME] =  
                      {"can00",
                       "can01",
                       "can02",
                       "can03",
                       "can04",
                       "can05",
                       "can06",
                       "can07",
                       "can08"};
                       
String FLS_name[3] =  {"finish",
                       "pit_entry",
                       "pit_exit"};
                       
String PIT_name[6] =  {"pit_length",
                       "pit_max_spd",
                       "pit_time",
                       "pit_acc",
                       "ir_beacon",
                       "gps_beacon"};
char buffer[1024];
PString datastring(buffer, sizeof(buffer));
char res[21];

static int num_cycle = 0;

/// \fn SetNulls
/// \brief Generate part of file names \n
/// \detailed 
/// Add nulls to filenum and return string with format 00001 \n
String SetNulls(){
  String n; 
  int numNulls = 0;
  if (fileNum > 65533)
     fileNum = 0;
  if (fileNum < 10000)
     numNulls = 1;
  if (fileNum < 1000)
     numNulls = 2;
  if (fileNum < 100)
     numNulls = 3;
  if (fileNum < 10)
     numNulls = 4;
  //generate next file name:
  while (numNulls){
    n = n + "0";
    numNulls--;
  }
  return n;
}

/// \fn incFileNum incFileNum
/// \brief Increment file number and generate file name \n
/// \detailed 
/// Return name file (ex. LOG00001.CSV) \n
void incFileNum() { 
  fileNum++;
  String s = "LOG" + SetNulls() + String(fileNum) + ".CSV";
  s.toCharArray(namefile,13);
}

/// \fn update_EEPROM
/// \brief Save in EEPROM data from config.jsn \n
/// \detailed
/// Write data blocks (number of bytes is defined in loglib.h) to EEPROM \n
void update_EEPROM() {
  EEPROM.setMemPool(memBase1, EEPROMSize);
  EEPROM.writeBlock(memBase1, CNF);
  EEPROM.writeBlock(memBase2, TPV);
  EEPROM.writeBlock(memBase3, ATT);
  EEPROM.writeBlock(memBase4, CAN);
  EEPROM.writeBlock(memBase5, FLS);
  EEPROM.writeBlock(memBase5, PIT);
}

/// \fn read_EEPROM
/// \brief Read config data from EEPROM \n
/// \detailed
/// Read data blocks (number of bytes is defined in loglib.h) from EEPROM \n
void read_EEPROM() {
  EEPROM.setMemPool(memBase1, EEPROMSize);
  EEPROM.readBlock(memBase1, CNF);
  EEPROM.readBlock(memBase2, TPV);
  EEPROM.readBlock(memBase3, ATT);
  EEPROM.readBlock(memBase4, CAN);
  EEPROM.readBlock(memBase5, FLS);
  EEPROM.readBlock(memBase6, PIT);
}

/// \fn parseJSON
/// \brief Parse config.jsn file from SD card \n
/// \detailed
/// 1.Read config.jsn file and parse it \n
/// 2.If parsing is failed print message in serial "parseObject() failed" \n
/// 3.If parsing is ok update EEPROM \n
void parseJSON() {
    char data, configData[number_JSON_object][500];
    int j = 0;
    unsigned int i = 0;
    int len[number_JSON_object];
    dataFile = SD.open(nameConfig, FILE_READ);
    if (dataFile){
        // make data to String;
        while (((data = dataFile.read()) >= 0)&&(i < number_JSON_object)){
             if (data==10){}
             else if (data==0x7D){
                 configData[i][j++] = data;
                 len[i] = j;
                 j = 0;
                 i++;
             }
             else{
                 configData[i][j++] = data;
             }
        }
        Serial.println(nameConfig);
        for (i = 0; i < number_JSON_object; i++){
            for (j = 0; j < len[i]; j++)
                Serial.print(configData[i][j]);
            Serial.println();
        }
    }
    else {
        Serial.println("config file is not found");
        config_open = false;
    }
    dataFile.close();
    if (config_open) {
      JsonObject& config1 = jsonBuffer1.parseObject(configData[0]);
      JsonObject& config2 = jsonBuffer2.parseObject(configData[1]);
      JsonObject& config3 = jsonBuffer3.parseObject(configData[2]);
      JsonObject& config4 = jsonBuffer4.parseObject(configData[3]);
      JsonObject& config5 = jsonBuffer5.parseObject(configData[4]);
      JsonObject& config6 = jsonBuffer6.parseObject(configData[5]);
      if (!(config1.success() && config2.success() && config3.success() && config4.success())) {
          Serial.println("parseObject() failed");
          parse_success = 0;
      }
      else{
          parse_success = 1;
      }
      classConfig[0] = config1["class"];
      for (i = 0; i < sizeof(CNF)/4; i++){
          CNF[i] = config1 [CNF_name[i]];
      };
      classConfig[1] = config2["class"];
      for (i = 0; i < sizeof(TPV); i++){
          TPV[i] = config2 [TPV_name[i]];
      };
      classConfig[2] = config3["class"];
      for (i = 0; i < sizeof(ATT); i++){
          ATT[i] = config3 [ATT_name[i]];
      };
      classConfig[3] = config4["class"];
      for (i = 0; i < NUM_CAN_FRAME; i++){
          CAN[i].en = config4 [CAN_name[i]][0];
          CAN[i].id = config4 [CAN_name[i]][1];
      };
      
      classConfig[4] = config5["class"];
      
      for (i = 0; i < 3; i++){
          FLS[i].en = config5 [FLS_name[i]][0];
          FLS[i].lat_A = config5 [FLS_name[i]][1];
          FLS[i].lon_A = config5 [FLS_name[i]][2];
          FLS[i].lat_B = config5 [FLS_name[i]][3];
          FLS[i].lon_B = config5 [FLS_name[i]][4];
          FLS[i].lineOutString = config5 [FLS_name[i]][5];
          if (strcmp(FLS[i].lineOutString, LineOut_name[0])==0)
              FLS[i].lineOut = 0;
          else if (strcmp(FLS[i].lineOutString, LineOut_name[1])==0)
              FLS[i].lineOut = 1;
          else if (strcmp(FLS[i].lineOutString, LineOut_name[2])==0)
              FLS[i].lineOut = 2;   
          FLS[i].maxSpeed = config5 [FLS_name[i]][6];
          FLS[i].minSpeed = config5 [FLS_name[i]][7];
      };    
      for (i = 0; i < 1; i++){
          PIT[i].pit_length = config6 [PIT_name[0]];
          PIT[i].pit_max_spd = config6 [PIT_name[1]];
          PIT[i].pit_time = config6 [PIT_name[2]];
          PIT[i].pit_acc = config6 [PIT_name[3]];
          PIT[i].ir_beacon = config6 [PIT_name[4]];
          PIT[i].gps_beacon = config6 [PIT_name[5]];
      };
      Serial.println("Update EEPROM");
      update_EEPROM();
   }
   else{
      Serial.println("Read EEPROM");
      read_EEPROM();
      parse_success = 1;
   }
}

/// \fn printJSON
/// \brief Print in serial config data \n
/// \detailed
/// Print CNF, TPV, ATT, CAN, FLS, PIT data \n
void printJSON(){
    Serial.print("CNF");
    Serial.print(" ");
    for (unsigned int i = 0; i < sizeof(CNF)/4; i++){
        Serial.print(CNF[i]);
        Serial.print(" ");
    };
    Serial.println();
    Serial.print("TPV");
    Serial.print(" ");
    for (unsigned int i = 0; i < sizeof(TPV); i++){
        Serial.print(TPV[i]);
        Serial.print(" ");
    };
    Serial.println();
    Serial.print("ATT");
    Serial.print(" ");
    for (unsigned int i = 0; i < sizeof(ATT); i++){
        Serial.print(ATT[i]);
        Serial.print(" ");
    };
    Serial.println();
    Serial.print("Canbus output enabled on frames:");
    Serial.print(" ");
    for (unsigned int i = 0; i < NUM_CAN_FRAME; i++){
        if (CAN[i].en){
        Serial.print(i);
        Serial.print(":");
        Serial.print(CAN[i].id,HEX);
        Serial.print(" ");
        }
    };
    Serial.println();
    Serial.print("FLS");
    Serial.println(" ");
    for (unsigned int i = 0; i < 3; i++){
        if (FLS[i].en)
            Serial.print("     ");
            Serial.print(FLS_name[i]);
            Serial.print(": ");
            Serial.print(FLS[i].lat_A, 7);
            Serial.print(" ");
            Serial.print(FLS[i].lon_A, 7);
            Serial.print(" ");
            Serial.print(FLS[i].lat_B, 7);
            Serial.print(" ");
            Serial.print(FLS[i].lon_B, 7);
            Serial.print(" ");
            Serial.print(FLS[i].lineOut);
            Serial.print(" ");
            Serial.print(FLS[i].maxSpeed, 7);
            Serial.print(" ");
            Serial.print(FLS[i].minSpeed, 7);
        Serial.println(" ");
    };
    Serial.print("PIT");
    Serial.println(" ");
    for (unsigned int i = 0; i < 1; i++){
        Serial.print("     ");
        Serial.print(PIT[i].pit_length, 7);
        Serial.print(" ");
        Serial.print(PIT[i].pit_max_spd, 7);
        Serial.print(" ");
        Serial.print(PIT[i].pit_time, 7);
        Serial.print(" ");
        Serial.print(PIT[i].pit_acc, 7);
        Serial.print(" ");
        Serial.print(PIT[i].ir_beacon);
        Serial.print(" ");
        Serial.print(PIT[i].gps_beacon);
        Serial.println(" ");
    };
    Serial.println();
}

/// \fn checkTPV 
/// \brief Return TPV name if enabled \n
/// 
String check_TPV(int num){
  if (TPV[num])
     return TPV_name[num]+",";
  else 
     return "";
}

/// \fn checkATT
/// \brief Return ATT name if enabled
/// 
String check_ATT(int num){
  if (ATT[num])
     return ATT_name[num]+",";
  else 
     return "";
}

/// \fn LogSetup
/// \brief Setup SD to log data \n
/// \detailed
/// 1. Setup pins MOSI, MISO, SCK for SD card \n
/// 2. If SD card is not detected power off led and set sd_datalog false \n
/// 3. If SD card is detected power on led and set sd_datalog true \n
/// 4. Parse config.jsn file and print data config \n
void LogSetup() {
 SPI.setMOSI(mosi);
 SPI.setMISO(miso);
 SPI.setSCK(sck);
 pinMode(chipSelect, OUTPUT);
 pinMode(ADAFRUITBLE_CS, OUTPUT);
 delay(1000);
 BTLEserial.begin();
 if ((!SD.begin(chipSelect))&&(file_log==true))
   {  
      Serial.println("File datalog enabled");    
      Serial.println("Card Not Present");
      led_on = LOW;
      digitalWrite(led,led_on);
      sd_datalog=false;
   }
 else
   {
      Serial.println("File datalog enabled");    
      Serial.println("Card Present");
      led_on = HIGH;
      digitalWrite(led,led_on);
      sd_datalog=true;
      card.init(SPI_FULL_SPEED, chipSelect);
   }
 parseJSON();
 printJSON();
 if (sd_datalog==true) {
    if (parse_success)
        log_output = CNF[0];
    if (log_output)
        create_newlog();
        datastring.begin();
 }//if 
 
}

/// \fn create_newlog
/// \brief Create newlog file \n
/// \detailed
/// 1. If file is presented on SD increment file num and create new file \n
/// 2. Open file and save header from TPV and ATT data \n
void create_newlog(){
    String header;
    while (SD.exists(namefile)) {      
        Serial.print(namefile);
        Serial.println(" file present."); 
        incFileNum();
    }
    Serial.print("Logging Data to "); 
    Serial.println(namefile);
    dataFile = SD.open(namefile, FILE_WRITE);           
    delay(100);
    if (dataFile){
        for (unsigned int i = 0; i < sizeof(TPV); i++){
           header = header + check_TPV(i);
        }
        header = "class," + header + "class,";
        for (unsigned int i = 0; i < sizeof(ATT); i++){
           header = header + check_ATT(i);
        }
        dataFile.println(header);
        Serial.println("Header wrote ok");
        dataFile.close();
    }
    else {
        Serial.println("Impossible to open datafile to write header");
        sd_datalog=0;
    }//if
}

/// \fn dataFloat
/// \brief Print float data to string \n
/// 
void dataFloat(float value, int mode){
    char outstr[21];
    dtostrf(value, 20, 12, outstr);
    char* str_without_space = del_space(outstr);
    if (gps.venus838data_raw.fixmode >= mode)
        datastring.print (str_without_space);
    datastring.print(",");
}

/// \fn dataFloatATT
/// \brief Print float data to string \n
/// 
void dataFloatATT(float value, int mode){
    char outstr[21];
    dtostrf(value, 20, 12, outstr);
    char* str_without_space = del_space(outstr);
    if (gps.venus838data_raw.fixmode >= mode)
        datastring.print (str_without_space);
    datastring.print(",");
}

/// \del_space
/// \brief delete space from string
/// 
char* del_space(char* src){
   int i, j;
   for(i = j = 0; src[i] != '\0'; i++)
        if(src[i] != ' ')
            res[j++] = src[i];
   res[j] = '\0';
   char *p = res;
   return p;
}

/// \fn LogTPV
/// \brief Print TPV data to tpvstring \n
/// 
void LogTPV(){
     //Serial.println("Print TPV object"); 
     if (!dataFile) 
          dataFile = SD.open(namefile, O_WRITE);
      datastring.print("TPV,");
      if(TPV[0])
          datastring.print("Venus838,");    
      if(TPV[1]){
          datastring.print(gps.venus838data_raw.fixmode);
          datastring.print(",");   
      };
      if(TPV[2]){
          datastring.print(gps.venus838data_raw.NumSV);
          datastring.print(",");  
      };
      if(TPV[3]){
          datastring.print(UTC_Time);
          datastring.print(",");
      };
      if(TPV[4])
          dataFloat(gps.venus838data_raw.Latitude, 1);
      if(TPV[5])
          dataFloat(gps.venus838data_raw.Longitude, 1);
      if(TPV[6])
          dataFloat(gps.venus838data_raw.SealevelAltitude, 2);
      if(TPV[7])
          dataFloat(gps.venus838data_filter.Latitude, 1);
      if(TPV[8])
          dataFloat(gps.venus838data_filter.Longitude, 1);
      if(TPV[9])
          dataFloat(gps.venus838data_filter.SealevelAltitude, 2);
      if(TPV[10])
          dataFloat(course_angle, 0);
      if(TPV[11])
          dataFloat(gps.venus838data_raw.velocity, 0);
      if(TPV[12])
          dataFloat(gps.venus838data_filter.velocity, 0);
      if(TPV[13])
          dataFloat(gps.venus838data_raw.gdop, 0);
      if(TPV[14])
          dataFloat(gps.venus838data_raw.pdop, 0);
      if(TPV[15])
          dataFloat(gps.venus838data_raw.hdop, 0);
      if(TPV[16])
          dataFloat(gps.venus838data_raw.vdop, 0);      
      if(TPV[17])
          dataFloat(gps.venus838data_raw.tdop, 0); 
      if(TPV[18]){
          datastring.print(checksums);  
          datastring.print(",");
      };
      //dataFile.print(datastring);
}

/// \fn LogATT
/// \brief Print ATT data to attstring \n
/// 
void LogATT(){
    att.heading = heading;
    att.pitch = pitch;
    att.yaw = yaw_rate;
    att.roll = roll;
    att.dip = inclination;
    att.mag_len = sqrt(sq(mx)+sq(my)+sq(mz));
    att.mag_x = mx;
    att.mag_y = my;
    att.mag_z = mz;
    att.acc_len = sqrt(sq(ax)+sq(ay)+sq(az));
    att.acc_x = ax;
    att.acc_y = ay;
    att.acc_z = az;
    att.gyro_x = gx;
    att.gyro_y = gy;
    att.gyro_z = gz;
    att.quat1 = q[0];
    att.quat2 = q[1];
    att.quat3 = q[2];
    att.quat4 = q[3];
    att.temp = temp;
    att.pressure = bmp280_pressure;
   // Serial.println("Print ATT object"); 
    if (!dataFile) 
         dataFile = SD.open(namefile, O_WRITE);
    datastring.print("ATT,");
    if(ATT[0])
        datastring.print("LSM9DS0TR,");
    if(ATT[1]){
        datastring.print(Delta_Time);
        datastring.print(",");
    };
    if(ATT[2])
        dataFloatATT(att.heading, 0);
    if(ATT[3])
        dataFloatATT(att.pitch, 0);
    if(ATT[4])
        dataFloatATT(att.yaw*YAW_SCALE, 0);
    if(ATT[5])
        dataFloatATT(att.roll, 0);
    if(ATT[6])
        dataFloatATT(att.dip, 0);
    if(ATT[7])
        dataFloatATT(att.mag_len, 0);
    if(ATT[8])
        dataFloatATT(att.mag_x, 0);
    if(ATT[9])
        dataFloatATT(att.mag_y, 0);
    if(ATT[10])
        dataFloatATT(att.mag_z, 0);
    if(ATT[11])
        dataFloatATT(att.acc_len, 0);
    if(ATT[12])
        dataFloatATT(att.acc_x*ACC_SCALE, 0);
    if(ATT[13])
        dataFloatATT(att.acc_y*ACC_SCALE, 0);
    if(ATT[14])
        dataFloatATT(att.acc_z*ACC_SCALE, 0);
    if(ATT[15])
        dataFloatATT(att.gyro_x, 0);
    if(ATT[16])
        dataFloatATT(att.gyro_y, 0);
    if(ATT[17])
        dataFloatATT(att.gyro_z, 0);
    if(ATT[18])
        dataFloatATT(att.quat1, 0);
    if(ATT[19])
        dataFloatATT(att.quat2, 0);
    if(ATT[20])
        dataFloatATT(att.quat3, 0); 
    if(ATT[21])
        dataFloatATT(att.quat4, 0);
    if(ATT[22])
        dataFloatATT(att.temp, 0);
    if(ATT[23])
        dataFloatATT(att.pressure, 0);
    num_cycle =  num_cycle + 1;
    datastring.println(); 
    dataFile.print(datastring);
    datastring.begin();
    if (num_cycle % 50 == 0){ //close file per 1 sec
        filesize = dataFile.size();
        dataFile.close();
        Serial.println("Datafile Saved");
        Serial.println(filesize);
        
    }
}

/// \fn LogATT_nosd
/// \brief Save att parameters to att struct when not logged on SD card
/// 
void LogATT_nosd()
{
    att.heading = heading;
    att.pitch = pitch;
    att.yaw = yaw;
    att.roll = roll;
    att.dip = inclination;
    att.mag_len = sqrt(sq(mx)+sq(my)+sq(mz));
    att.mag_x = mx;
    att.mag_y = my;
    att.mag_z = mz;
    att.acc_len = sqrt(sq(ax)+sq(ay)+sq(az));
    att.acc_x = ax;
    att.acc_y = ay;
    att.acc_z = az;
    att.gyro_x = gx;
    att.gyro_y = gy;
    att.gyro_z = gz;
    att.quat1 = q[0];
    att.quat2 = q[1];
    att.quat3 = q[2];
    att.quat4 = q[3];
    att.temp = temp;
    att.pressure = bmp280_pressure;
}

/// \fn 
/// \brief Fuction that need to get Julian Date from GPS time
/// 
boolean TIMECONV_GetJulianDateFromGPSTime(
   const int              gps_week,      //!< GPS week (0-1024+)             [week]
   const unsigned long    gps_tow,       //!< GPS time of week (0-604800.0)  [s]
   const int              utc_offset,    //!< Integer seconds that GPS is ahead of UTC time, always positive [s]
   double                 &julian_date    //!< Number of days since noon Universal Time Jan 1, 4713 BCE (Julian calendar) [days]
   )
{   
   if( gps_tow < 0.0  || gps_tow > 604800.0 )
     return false;  
   // GPS time is ahead of UTC time and Julian time by the UTC offset
   julian_date = (double(gps_week) + (gps_tow-utc_offset)/604800.0)*7.0 + TIMECONV_JULIAN_DATE_START_OF_GPS_TIME;  
   return true;
}

boolean TIMECONV_GetUTCTimeFromJulianDate(
   const double        julian_date,  //!< Number of days since noon Universal Time Jan 1, 4713 BCE (Julian calendar) [days]
   unsigned short*     utc_year,     //!< Universal Time Coordinated    [year]
   unsigned char*      utc_month,    //!< Universal Time Coordinated    [1-12 months] 
   unsigned char*      utc_day,      //!< Universal Time Coordinated    [1-31 days]
   unsigned char*      utc_hour,     //!< Universal Time Coordinated    [hours]
   unsigned char*      utc_minute,   //!< Universal Time Coordinated    [minutes]
   float*              utc_seconds   //!< Universal Time Coordinated    [s]
   )
 {
   int a, b, c, d, e; // temporary values
   
   unsigned short year;  
   unsigned char month;
   unsigned char day;
   unsigned char hour;
   unsigned char minute;        
   unsigned char days_in_month = 0;  
   double td; // temporary double
   double seconds;
   boolean result;
 
   // Check the input.
   if( julian_date < 0.0 )
     return false;
   
   a = (int)(julian_date+0.5);
   b = a + 1537;
   c = (int)( ((double)b-122.1)/365.25 );
   d = (int)(365.25*c);
   e = (int)( ((double)(b-d))/30.6001 );
   td      = b - d - (int)(30.6001*e) + fmod( julian_date+0.5, 1.0 );   // [days]
   day     = (unsigned char)td;     
   td     -= day;
   td     *= 24.0;        // [hours]
   hour    = (unsigned char)td;
   td     -= hour;
   td     *= 60.0;        // [minutes]
   minute  = (unsigned char)td;
   td     -= minute;
   td     *= 60.0;        // [s]
   seconds = td;
   month   = (unsigned char)(e - 1 - 12*(int)(e/14));
   year    = (unsigned short)(c - 4715 - (int)( (7.0+(double)month) / 10.0 ));
   
   // check for rollover issues
   if( seconds >= 60.0 )
   {
     seconds -= 60.0;
     minute++;
     if( minute >= 60 )
     {
       minute -= 60;
       hour++;
       if( hour >= 24 )
       {
         hour -= 24;
         day++;
         
         result = TIMECONV_GetNumberOfDaysInMonth( year, month, &days_in_month );
         if( result == false )
           return false;
         
         if( day > days_in_month )
         {
           day = 1;
           month++;
           if( month > 12 )
           {
             month = 1;
             year++;
           }
         }
       }
     }
   }   
   
   *utc_year       = year;
   *utc_month      = month;
   *utc_day        = day;
   *utc_hour       = hour;
   *utc_minute     = minute;
   *utc_seconds    = (float)seconds;   
 
   return true;
}

boolean TIMECONV_GetNumberOfDaysInMonth(
   const unsigned short year,        //!< Universal Time Coordinated    [year]
   const unsigned char month,        //!< Universal Time Coordinated    [1-12 months] 
   unsigned char* days_in_month      //!< Days in the specified month   [1-28|29|30|31 days]
   )
{
   boolean is_a_leapyear;
   unsigned char utmp = 0;
   
   is_a_leapyear = TIMECONV_IsALeapYear( year );
   
   switch(month)
   {
   case  1: utmp = TIMECONV_DAYS_IN_JAN; break;
   case  2: if( is_a_leapyear ){ utmp = 29; }else{ utmp = 28; }break;    
   case  3: utmp = TIMECONV_DAYS_IN_MAR; break;
   case  4: utmp = TIMECONV_DAYS_IN_APR; break;
   case  5: utmp = TIMECONV_DAYS_IN_MAY; break;
   case  6: utmp = TIMECONV_DAYS_IN_JUN; break;
   case  7: utmp = TIMECONV_DAYS_IN_JUL; break;
   case  8: utmp = TIMECONV_DAYS_IN_AUG; break;
   case  9: utmp = TIMECONV_DAYS_IN_SEP; break;
   case 10: utmp = TIMECONV_DAYS_IN_OCT; break;
   case 11: utmp = TIMECONV_DAYS_IN_NOV; break;
   case 12: utmp = TIMECONV_DAYS_IN_DEC; break;
   default: return false; break;    
   }
   
   *days_in_month = utmp;
 
   return true;
 }

boolean TIMECONV_IsALeapYear(const unsigned short year )
{
   boolean is_a_leap_year = false;
 
   if( (year%4) == 0 )
   {
     is_a_leap_year = true;
     if( (year%100) == 0 )
     {
       if( (year%400) == 0 )
       {
         is_a_leap_year = true;
       }
       else
       {
         is_a_leap_year = false;
       }
     }
   }
   if( is_a_leap_year )
   {
     return true;
   }
   else
   {
     return false;
   }
}

String GetUTCTime (unsigned int num_week, unsigned long timeofweek){
  String UTC_Time, UTC_Date;
  unsigned short    utc_year;
  unsigned char     sec_, utc_month, utc_day, utc_hour, utc_minute;
  float   utc_seconds;
  double julian_date;
  sec_ = timeofweek % 100;
  timeofweek = timeofweek/100;
  TIMECONV_GetJulianDateFromGPSTime(num_week, timeofweek, 17, julian_date);
  TIMECONV_GetUTCTimeFromJulianDate(julian_date, &utc_year, &utc_month, &utc_day, &utc_hour, &utc_minute, &utc_seconds);
  UTC_Date = String(utc_year) + '-' + String(utc_month) + '-' + String(utc_day);
  UTC_Time = 'T' + null_add(utc_hour) + String(utc_hour) + ':' + null_add(utc_minute)  + String(utc_minute) + ':' + null_add(utc_seconds) +
  String(utc_seconds) + '.' + null_add(sec_) + String(sec_);
  return (UTC_Date + UTC_Time);
}

String null_add(int value){
  String null;
  if (value < 10)
     null = '0';
  else 
     null = "";
  return null;
}

String GetDeltaTime(float time){
  unsigned int time_ms, time_sec, time_min, time_hour;
  time_ms = (int(time) % 1000)/10;
  time_sec = time / 1000;
  time_min = time_sec / 60;
  time_sec = time_sec % 60;
  time_hour = time_min / 60;
  time_min = time_min % 60;
  Delta_Time = null_add(time_hour) + String(time_hour) + ':' + null_add(time_min) + String(time_min) + ':' +
  null_add(time_sec) + String(time_sec) + ':' + null_add(time_ms) + String(time_ms);
  return Delta_Time;
}

