#include <SPI.h>
//#include <Wire.h>
#include <Arduino_USBHostMbed5.h>
#include <FATFileSystem.h>

USBHostMSD msd;
mbed::FATFileSystem usb("usb");

// SRAM codes (for 1Mb and 2Mb)
#define RDSR        5
#define WRSR        1
#define READ        3
#define WRITE       2
#define WREN        6
#define WRDI        8

// SRAM code 
#define SEQ_MODE  (0x40 | HOLD)

// SRAM Hold line disabled when HOLD == 1
#define HOLD 0 // HOLD 1 FOR 1MBit, and 0 for 2MBit

//Settings

// Set maxram for 2Mbit - 262144 bytes
SPISettings settingsA(6000000, MSBFIRST, SPI_MODE0);
uint32_t maxram = 262144;
long     startAddr = 0x00;

// Other parameters
uint32_t number = 8;

// FAke picture parameters
uint32_t pic_size = 8387638;
uint32_t last_size;
uint32_t used_number;
long filenum = 0;

// byte     cs_Pins[8] =  { 2,3,4,5,6,7,8,9 };
byte     cs_Pins[36] = {2, 3, 4, 5, 6, 7, 8, 9, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 48, 50, 52};

void Spi_SRAM_WriteSeq(uint32_t ramsize, uint8_t cs_pin, FILE *f) {
  uint32_t i;
  uint8_t c;
  SPI.beginTransaction(settingsA);
  digitalWrite(cs_pin, LOW);
  SPI.transfer(WRITE);
  SPI.transfer((uint8_t)(startAddr >> 16) & 0xff);
  SPI.transfer((uint8_t)(startAddr >> 8) & 0xff);
  SPI.transfer((uint8_t)(startAddr & 0xff));
  for (i = 0; i < ramsize; i++) {
    c =fgetc(f);
   // Serial.println(c);
    SPI.transfer(c);
  }
  digitalWrite(cs_pin, HIGH);
  SPI.endTransaction();
}

uint32_t Spi_SRAM_ReadSeq(uint32_t ramsize, uint8_t cs_pin, FILE *f) {
  uint8_t read_byte,c;
  uint32_t num = 0;
  uint32_t i;
  SPI.beginTransaction(settingsA);
  digitalWrite(cs_pin, LOW);
  SPI.transfer(READ);
  SPI.transfer((uint8_t)(startAddr >> 16) & 0xff);
  SPI.transfer((uint8_t)(startAddr >> 8) & 0xff);
  SPI.transfer((uint8_t)(startAddr & 0xff));
  for (i = 0; i < ramsize; i++) {
   read_byte = SPI.transfer(0x00);
   c = fgetc(f);
   if (read_byte != c) {
       num++;
   } 
  }
  digitalWrite(cs_pin, HIGH);
  SPI.endTransaction();
  return num;
}

uint32_t Spi_SRAM_OutSeq(uint32_t ramsize, uint8_t cs_pin, FILE *o) {
  uint8_t read_byte,err;
  uint32_t i;
  SPI.beginTransaction(settingsA);
  digitalWrite(cs_pin, LOW);
  SPI.transfer(READ);
  SPI.transfer((uint8_t)(startAddr >> 16) & 0xff);
  SPI.transfer((uint8_t)(startAddr >> 8) & 0xff);
  SPI.transfer((uint8_t)(startAddr & 0xff));
  for (i = 0; i < ramsize; i++) {
   read_byte = SPI.transfer(0x00);
   err = fputc(read_byte,o);
   //check error
   if (err < 0) {
      Serial2.println("Fail :(");
      error("error: %s (%d)\n", strerror(errno), -errno);
    }
    //
   //Serial.println(err);
  }
  digitalWrite(cs_pin, HIGH);
  SPI.endTransaction();
}

void setup()
{
    int k;  

    Serial1.begin(9600);
    Serial2.begin(9600);
    SPI.begin();

    while (!Serial2) {
    ; // wait for serial port to connect. Needed for native USB port only
    }

// Set up CS_Pins - digital and initially HIGH
  for (k=0; k<number; k++) {
    pinMode(cs_Pins[k], OUTPUT);
    digitalWrite(cs_Pins[k], HIGH);
  }  

// Set each SRAM chip into SEQ mode
    for (k=0; k<number; k++) {  
     digitalWrite(cs_Pins[k], LOW);
     SPI.beginTransaction(settingsA);
     SPI.transfer(WRSR);
     SPI.transfer(SEQ_MODE);
     digitalWrite(cs_Pins[k], HIGH); 
     SPI.endTransaction();
    } 

  // Looking at file on the USB device - first time find the size
  pinMode(PA_15, OUTPUT); //enable the USB-A port
  digitalWrite(PA_15, HIGH);   
  delay(2500);

  while (!msd.connect()) {
    delay(1000);
    Serial2.println("Waiting for USB...");
  }

  Serial2.println("Mounting USB device...");
  int err =  usb.mount(&msd);
  if (err) {
    Serial2.print("Error mounting USB device ");
    Serial2.println(err);
    while (1);
  }
  
/*  Serial2.println("Open file...");
  FILE *f = fopen("/usb/Arduino.txt", "r+");
  int c;
  int32_t n=0;
  //digitalWrite(PJ_13,LOW); //Pull LOW to light
// Tests for EOL and then reads single character, increments count by 1
  while(!feof(f)) {
    c =fgetc(f);
    n++;
  }
  Serial2.print("Number of bytes: ");
  Serial2.println(n);

  Serial2.println("File closing");
  fflush(stdout);
  err = fclose(f);
  if (err < 0) {
    Serial2.print("fclose error:");
    Serial2.print(strerror(errno));
    Serial2.print(" (");
    Serial2.print(-errno);
    Serial2.print(")");
  } else {
    Serial2.println("File closed");
  }*/
  used_number = (int) (floor(pic_size/maxram)+1);
  last_size = (int) (pic_size-((used_number-1)*maxram));
  Serial2.println(used_number);
  Serial2.println(last_size);
  
}

void loop() {
uint32_t numerr,totalerr;
int k;
int err;
char outname[18];
FILE *f;
FILE *o;
byte incomingByte;

if (Serial2.available() > 0) {
  // read the incoming byte:
  incomingByte = Serial2.read();
  //Serial.println(incomingByte);
}

switch (incomingByte)

{
    case 'w': // *** Write using SEQ mode and display errors (+ count)
      Serial2.println("Begin WRITE");
      Serial2.println("Open file to WRITE");
      f = fopen("/usb/image.bmp", "r+");
      for (k=0; k<used_number; k++) {
       if (k<used_number-1) {
        Serial2.println(k);
        Spi_SRAM_WriteSeq(maxram, cs_Pins[k],f);
       } else {
        Serial2.println(k);
        Serial2.println(last_size);
        Spi_SRAM_WriteSeq(last_size, cs_Pins[k],f);
       }
      }
      Serial2.println("Finish WRITE");
      Serial2.println("File closing for WRITE");
      fflush(stdout);
      err = fclose(f);
      incomingByte = '*';
      break;

    case 'r':
       Serial2.println("Begin READ");
       Serial2.println("Open file for READ Check");
       f = fopen("/usb/image.bmp", "r+");
       totalerr = 0;
       for (k=0; k<used_number; k++) {
       if (k<used_number-1) {
         numerr = Spi_SRAM_ReadSeq(maxram,cs_Pins[k],f);
         totalerr = totalerr + numerr;
         Serial2.print(k);
         Serial2.print(":");
         Serial2.print(cs_Pins[k]);
         Serial2.print(":");
         Serial2.println(numerr);
       } else {
         Serial2.println(k);
         Serial2.println(last_size);
         numerr = Spi_SRAM_ReadSeq(last_size,cs_Pins[k],f);
         totalerr = totalerr + numerr;
         Serial2.print(k);
         Serial2.print(":");
         Serial2.print(cs_Pins[k]);
         Serial2.print(":");
         Serial2.println(numerr);
        }
       }
       Serial2.println("Finish READ");
       Serial2.println("File closing for READ");
       fflush(stdout);
       err = fclose(f);
       Serial2.print("Errors: ");
       Serial2.println(totalerr);
       incomingByte = '*';
       break;

    case 'o':
       // Additional lines cf "w" and "r" as writing not reading - from Arduino Read/Write example
       mbed::fs_file_t file;
       struct dirent *ent; 
       
       Serial2.println("Begin OUTPUT");
       Serial2.println("Open file for OUPUT");
       sprintf(outname, "/usb/image%04d.bmp", filenum);
       Serial2.print("Open file for OUPUT:");
       Serial2.println(outname);
       o = fopen(outname, "w+");
       filenum++;
       for (k=0; k<used_number; k++) {
       if (k<used_number-1) {
         Serial2.println(k);
         Spi_SRAM_OutSeq(maxram,cs_Pins[k],o);
       } else {
         Serial2.println(k);
         Serial2.println(last_size);
         Spi_SRAM_OutSeq(last_size,cs_Pins[k],o);
        }
       }
       Serial2.println("Finish OUTPUT");
       Serial2.println("File closing for OUTPUT");
       fflush(stdout);
       err = fclose(o);
       incomingByte = '*';
       break;

    case 's': // *** Check this is SRAMs
      Serial2.println("SRAM");
      incomingByte = '*';
      break;
}



}
