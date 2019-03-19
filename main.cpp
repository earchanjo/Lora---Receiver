#include <Arduino.h>
#include <SPI.h>
#include <RH_RF95.h>
#include <EEPROM.h>
#include "crc.h"

#define  HEADER_LENGTH                                    4
#define  EEPROM_LSB0_PACKAGES_RECEIVED_SECTOR             0
#define  EEPROM_LSB1_PACKAGES_RECEIVED_SECTOR             1
#define  EEPROM_LSB0_PACKAGES_INCORRUPTED_SECTOR          2
#define  EEPROM_LSB1_PACKAGES_INCORRUPTED_SECTOR (uint8_t)3

#define  BEGIN_PACKAGE_INDEX_FIRST_HEADER                 0
#define  BEGIN_PACKAGE_INDEX_SESSION                      4
#define  BEGIN_PACKAGE_INDEX_LSB0_NUMBER_MAIN_PACKAGES    5
#define  BEGIN_PACKAGE_INDEX_LSB1_NUMBER_MAIN_PACKAGES    6
#define  BEGIN_PACKAGE_INDEX_CRC                          7
#define  BEGIN_PACKAGE_LENGTH                    (uint8_t)8

#define  MAIN_PACKAGE_INDEX_FIRST_HEADER                  0
#define  MAIN_PACKAGE_INDEX_SECOND_HEADER                 4
#define  MAIN_PACKAGE_INDEX_LSB0_ID_PACKAGE               8
#define  MAIN_PACKAGE_INDEX_LSB1_ID_PACKAGE               9
#define  MAIN_PACKAGE_INDEX_DUMMY                        10
#define  MAIN_PACKAGE_INDEX_CRC                          19
#define  MAIN_PACKAGE_LENGTH                             20

#define END_PACKAGE_INDEX_FIRST_HEADER                    0
#define END_PACKAGE_INDEX_CRC                             4
#define END_PACKAGE_LENGTH                       (uint8_t)5

#define ARRAY_TO_SEND_BACK_LENGTH                         4

#define ARRAY_RECEIVED_LENGTH                            10
#define TELEMETRY_FREQUENCY                           900.0

/*
    1- Checa o primeiro header, se falhar, checa o segundo 
*/
/* PACOTE INICIAL: {"M","N","B","G",SESSION,"CONFIG",NUMBER_ OF PACKAGES,CRC};
   PACOTE PRINCIPAL: {"M","N","N","A","M","N","N","A", ID_PACKAGE,DUMMY,CRC}
   PACOTE FINAL: {"M","N","E","N",FINAL_ID_PACKAGE, CRC}
   SESSION WILL BE ADDRES TO EEPROM SAVE NUMBERS OF PACKAGES AND NUMBER OF INTEGER PACKAGES
*/


RH_RF95 recep(4, 3);

void setup() {
    
    Serial.begin(9600);
    while(!Serial);

    if(!recep.init())
    {
        Serial.println("RECEPTOR_INIT FAILED");
    }
    else
        Serial.println("System initialized!");

    recep.setFrequency(TELEMETRY_FREQUENCY);
    
}

uint8_t arrayReceived[MAIN_PACKAGE_LENGTH];    // array to get received package, that have max length of all package
uint8_t lenArrayReceived = MAIN_PACKAGE_LENGTH;

// Variables to count packages received, and if then has arrived incorrupted
uint16_t packagesReceived = 0;
uint16_t packagesIncorrupted = 0;

// The headers each types of packages for check when package arrived 
const char beginPackageHeader[4] = {'M','N','B','G'};
const char mainPackageHeader[8] = {'M','N','M','A','M','N','M','A'};
const char endPackageHeader[4] = {'M','N','E','N'};
uint8_t arrayToSendBack[4] = {'M', 'N', 'R', 'V'};
uint8_t arrayToSendBackEnd[3] = {'E', 'N', 'D'};


void loop() {

    // WAIT MESSAGE  
    Serial.println("Waiting for package..");
    if (recep.waitAvailableTimeout(3000))     
    {   
        
        if (recep.recv(arrayReceived, &lenArrayReceived))              
        { 
    
            if (!memcmp(arrayReceived, beginPackageHeader, HEADER_LENGTH))   //this is BEGIN PACKAGE received
            {   
                //Serial.println(crc8(arrayReceived, BEGIN_PACKAGE_LENGTH));

                if(arrayReceived[BEGIN_PACKAGE_INDEX_CRC] == crc8(arrayReceived, BEGIN_PACKAGE_LENGTH - 1))
                {    // Check if begin package arrived incorrupt
                    Serial.println("BEGIN PACKAGE RECEIVED. SENDING RESPONSE MESSAGE...");
          
                    EEPROM.write(arrayReceived[BEGIN_PACKAGE_INDEX_SESSION] * 4 + EEPROM_LSB0_PACKAGES_RECEIVED_SECTOR, 0);
                    EEPROM.write(arrayReceived[BEGIN_PACKAGE_INDEX_SESSION] * 4 + EEPROM_LSB1_PACKAGES_RECEIVED_SECTOR, 0);
                    EEPROM.write(arrayReceived[BEGIN_PACKAGE_INDEX_SESSION] * 4 + EEPROM_LSB0_PACKAGES_INCORRUPTED_SECTOR, 0);
                    EEPROM.write(arrayReceived[BEGIN_PACKAGE_INDEX_SESSION] * 4 + EEPROM_LSB1_PACKAGES_INCORRUPTED_SECTOR, 0);

                    recep.send(arrayToSendBack, sizeof(arrayToSendBack));
                }
            }
            else if (!memcmp(arrayReceived + MAIN_PACKAGE_INDEX_FIRST_HEADER,  mainPackageHeader, HEADER_LENGTH) ||
            (!memcmp(arrayReceived + MAIN_PACKAGE_INDEX_SECOND_HEADER, mainPackageHeader, HEADER_LENGTH)))     // this is Main package received
            {   
                uint8_t a = crc8(arrayReceived,MAIN_PACKAGE_LENGTH - 1);
                Serial.print("main package crc; ");
                Serial.println(arrayReceived[MAIN_PACKAGE_INDEX_CRC]);

                Serial.print("crc8: ");
                Serial.println(a);
                if (arrayReceived[MAIN_PACKAGE_INDEX_CRC] == a)  // Check if Main package is incorrupted
                {
                    packagesIncorrupted++;
                    uint8_t packagesIncorrupted_LSB1 = packagesIncorrupted >> 8;
                    uint8_t packagesIncorrupted_LSB0 = packagesIncorrupted;

                    // SAVE NUMBER OF INCORRUPTED PACKAGES RECEIVED ON EEPROM
          
                    EEPROM.write(arrayReceived[BEGIN_PACKAGE_INDEX_SESSION] + EEPROM_LSB0_PACKAGES_INCORRUPTED_SECTOR, packagesIncorrupted_LSB0);
                    EEPROM.write(arrayReceived[BEGIN_PACKAGE_INDEX_SESSION] + EEPROM_LSB1_PACKAGES_INCORRUPTED_SECTOR, packagesIncorrupted_LSB1);

                    //PRINT ON SERIAL PORT CURRENT PACKAGE
                    
                    Serial.print("Incorrupetd packages Received: ");
                    Serial.println(packagesIncorrupted);


                    Serial.println("Package Received");
                }

                else
                {
                    packagesReceived ++;
                    uint8_t packagesReceived_LSB1 = packagesReceived >> 8;      // USE LSB TO SAVE ON EEPROM
                    uint8_t packagesReceived_LSB0 = packagesReceived;                      
        
                    // SAVE NUMBER OF PACKAGES RECEIVED ON EEPROM

                    EEPROM.write(arrayReceived[BEGIN_PACKAGE_INDEX_SESSION] + EEPROM_LSB0_PACKAGES_RECEIVED_SECTOR, packagesReceived_LSB0);
                    EEPROM.write(arrayReceived[BEGIN_PACKAGE_INDEX_SESSION] + EEPROM_LSB1_PACKAGES_RECEIVED_SECTOR, packagesReceived_LSB1);   

                    //PRINT ON SERIAL PORT CURRENT PACKAGE
                    // We have joined the package lsb1 and lsb0
                    Serial.print("Incorrupted packages Received: ");
                    Serial.println(packagesIncorrupted);

                    Serial.println("Package Received");
                }
       
            }
            else if (!memcmp(arrayReceived, endPackageHeader, HEADER_LENGTH))
            { 
                // RECEIVED END PACKAGE AND CHECK HIS INTEGRITY

                if (!memcmp(arrayReceived, endPackageHeader, HEADER_LENGTH) && 
                   (crc8(arrayReceived, END_PACKAGE_LENGTH - 1) == arrayReceived[END_PACKAGE_INDEX_CRC])) 
                {
                    packagesIncorrupted++;
                    uint8_t packagesIncorrupted_LSB1 = packagesIncorrupted >> 8;
                    uint8_t packagesIncorrupted_LSB0 = packagesIncorrupted;

                    EEPROM.write(arrayReceived[BEGIN_PACKAGE_INDEX_SESSION] + EEPROM_LSB0_PACKAGES_INCORRUPTED_SECTOR, packagesIncorrupted_LSB0);
                    EEPROM.write(arrayReceived[BEGIN_PACKAGE_INDEX_SESSION] + EEPROM_LSB1_PACKAGES_INCORRUPTED_SECTOR, packagesIncorrupted_LSB1);
                    
                    //PRINT ON SERIAL PORT CURRENT PACKAGE
                    Serial.print("Packages Received: ");
                    Serial.println(packagesReceived);

                    Serial.print("Number of Corrupted packages: ");
                    Serial.println((packagesReceived - packagesIncorrupted));

                    Serial.println("number of incorrupted packages:");
                    Serial.println(packagesIncorrupted);

                    Serial.print("End Package Received");

                    recep.send(arrayToSendBackEnd, sizeof(arrayToSendBackEnd));
                }
                else
                {
                    packagesReceived ++;
                    uint8_t packagesReceived_LSB1 = packagesReceived >> 8;
                    uint8_t packagesReceived_LSB0 = packagesReceived;

                    EEPROM.write(arrayReceived[BEGIN_PACKAGE_INDEX_SESSION] + EEPROM_LSB0_PACKAGES_RECEIVED_SECTOR, packagesReceived_LSB0);
                    EEPROM.write(arrayReceived[BEGIN_PACKAGE_INDEX_SESSION] + EEPROM_LSB1_PACKAGES_RECEIVED_SECTOR, packagesReceived_LSB1);

                    //PRINT ON SERIAL PORT CURRENT PACKAGE
                    Serial.print("Packages Received: ");
                    Serial.println(packagesReceived);

                    Serial.print("Number of Corrupted packages: ");
                    Serial.println((packagesReceived - packagesIncorrupted));

                    Serial.println("number of incorrupted packages:");
                    Serial.println(packagesIncorrupted);

                    Serial.print("End Package Received");

                    recep.send(arrayToSendBackEnd, sizeof(arrayToSendBackEnd));
                    
                }
       
       
                Serial.println("End Package Received");
            }
     
        }
        else
        {
            Serial.println("Failed to Receive the package");      
        }
    }
    else
    {
        Serial.println("No Response...");           
    }
    delay(400);           // a simple and cool delay 
}