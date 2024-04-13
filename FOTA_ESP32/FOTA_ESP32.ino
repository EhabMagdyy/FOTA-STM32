/*****************************************************************************************************************************
**********************************    Author  : Ehab Magdy Abdullah                      *************************************
**********************************    Linkedin: https://www.linkedin.com/in/ehabmagdyy/  *************************************
**********************************    Youtube : https://www.youtube.com/@EhabMagdyy      *************************************
******************************************************************************************************************************/

/*******************************************   Included Libraries   **********************************************************/
#include <LittleFS.h>
#include <Arduino.h>
#include <Firebase_ESP_Client.h>
#include <PubSubClient.h>
#include <addons/TokenHelper.h>

/**************************************************  MACROS - Variables  ****************************************************/
#if defined(ESP32) || defined(ARDUINO_RASPBERRY_PI_PICO_W)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#elif __has_include(<WiFiNINA.h>)
#include <WiFiNINA.h>
#elif __has_include(<WiFi101.h>)
#include <WiFi101.h>
#elif __has_include(<WiFiS3.h>)
#include <WiFiS3.h>
#endif

/* 1. Define the WiFi credentials */
#define WIFI_SSID             "2001"
#define WIFI_PASSWORD         "19821968"

/* 2. Define the API Key */
#define API_KEY               "AIzaSyDII_Pc2hh8JP-FtZQcAWmwyxonHVJge-4"

/* 3. Define the user Email and password that alreadey registerd or added in your project */
#define USER_EMAIL            "ehabmagdy0404@gmail.com"
#define USER_PASSWORD         "123456"

/* 4. Define the Firebase storage bucket ID e.g bucket-name.appspot.com */
#define STORAGE_BUCKET_ID     "fota-1c3fe.appspot.com"

#define NOT_ACKNOWLEDGE       0xAB
#define JUMP_SECCEDDED        0x01
#define ERASE_SUCCEDDED       0x03
#define WRITE_SUCCEDDED       0x01

const char* mqtt_server = "test.mosquitto.org";

/* Command packet in communication with STM32 */
uint8_t packet[100];          

// Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
WiFiMulti multi;
#endif

WiFiClient espClient;
PubSubClient client(espClient);

/******************************************************    Setup    **********************************************************/
void setup()
{
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1);

#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
    multi.addAP(WIFI_SSID, WIFI_PASSWORD);
    multi.run();
#else
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
#endif

    Serial.print("Connecting to Wi-Fi");
    unsigned long ms = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(300);
#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
        if (millis() - ms > 10000)
            break;
#endif
    }
    Serial.println();
    Serial.println("Connected");

    Serial.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);

    /* Assign the api key (required) */
    config.api_key = API_KEY;

    /* Assign the user sign in credentials */
    auth.user.email = USER_EMAIL;
    auth.user.password = USER_PASSWORD;

#if defined(ARDUINO_RASPBERRY_PI_PICO_W)
    config.wifi.clearAP();
    config.wifi.addAP(WIFI_SSID, WIFI_PASSWORD);
#endif

    /* Assign the callback function for the long running token generation task */
    config.token_status_callback = tokenStatusCallback; 

    // Comment or pass false value when WiFi reconnection will control by your code or third party library e.g. WiFiManager
    Firebase.reconnectNetwork(true);
    fbdo.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);
    /* Assign download buffer size in byte */
    config.fcs.download_buffer_size = 2048;
    Firebase.begin(&config, &auth);

    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);

    pinMode(2, OUTPUT);
}

/****************************************************    Loop    ************************************************************/
void loop()
{
    if(!client.connected()) { reconnect(); }
    client.loop();
}


/************************************ The Firebase Storage download callback function ****************************************/
void fcsDownloadCallback(FCS_DownloadStatusInfo info)
{
    if (info.status == firebase_fcs_download_status_init)
    {
        Serial.printf("Downloading file %s (%d) to %s\n", info.remoteFileName.c_str(), info.fileSize, info.localFileName.c_str());
    }
    else if (info.status == firebase_fcs_download_status_download)
    {
        Serial.printf("Downloaded %d%s, Elapsed time %d ms\n", (int)info.progress, "%", info.elapsedTime);
    }
    else if (info.status == firebase_fcs_download_status_complete)
    {
        Serial.println("Download completed\n");
    }
    else if (info.status == firebase_fcs_download_status_error)
    {
        Serial.printf("Download failed, %s\n", info.errorMsg.c_str());
    }
}


/****************************************************** Reconnect to MQTT *****************************************************/
void reconnect() 
{ 
  while(!client.connected()) 
  {
      Serial.println("Attempting MQTT connection...");

      if(client.connect("ESPClient")) 
      {
          Serial.println("Connected");
          client.subscribe("/FOTA/Boot");
      } 
      else 
      {
          Serial.print("Failed, rc=");
          Serial.print(client.state());
          Serial.println(" try again in 5 seconds");
          delay(5000);
      }
    }
}


/******************************************************** Node-Red Callback ****************************************************/
void callback(char* topic, byte* payload, unsigned int length) 
{ 
    byte recCommand = 0;
    Serial.print("Message arrived [");
    Serial.print(topic);
    Serial.print("]: ");
    recCommand = payload[0];
    Serial.println(recCommand);
    
    switch(recCommand)
    {
      case 'G':  Send_CMD_Read_CID();               break;
      case 'R':  Send_CMD_Read_Protection_Level();  break;
      case 'J':  Send_CMD_Jump_To_Application();    break;
      case 'E':  Send_CMD_Erase_Flash();            break;
      case 'U':  Send_CMD_Upload_Application();     break;
    }
}


/******************************************** Communication with bootloader *****************************************************/
void SendCMDPacketToBootloader(uint8_t packetLength)
{
    for(uint8_t dataIndex = 0 ; dataIndex < packetLength ; dataIndex++){
        Serial2.write(packet[dataIndex]);
        Serial.print(packet[dataIndex], HEX);
        Serial.print(" ");
    }
    Serial.println("\n");
}

void ReceiveReplayFromBootloader(uint8_t packetLength)
{
    for(uint8_t dataIndex = 0 ; dataIndex < packetLength ; dataIndex++)
    {
        while(!Serial2.available());
        packet[dataIndex] = Serial2.read();
        if(NOT_ACKNOWLEDGE == packet[0]) break;
    }
}


/***************************************** Bootloader Supported Commands' Functions ********************************************/
void Send_CMD_Read_CID()
{
    char Replay[16] = {0};
    String sReplay = "Chip ID: 0x";
    
    packet[0] = 5;
    packet[1] = 0x10;
    *((uint32_t*)((uint8_t*)packet + 2)) = calculateCRC32((uint8_t*)packet, 2);
    SendCMDPacketToBootloader(6);
    ReceiveReplayFromBootloader(2);

    if(NOT_ACKNOWLEDGE == packet[0]){ sReplay = "NACK"; }
    else{ sReplay += String(packet[1], HEX);   sReplay += String(packet[0], HEX); }

    sReplay.toCharArray(Replay, sReplay.length()+1);
    client.publish("/FOTA/BootReply", Replay, false); 
}

void Send_CMD_Read_Protection_Level()
{
    char Replay[16] = {0};
    String sReplay = "ProtecLvl: ";

    packet[0] = 5;
    packet[1] = 0x11;
    *((uint32_t*)((uint8_t*)packet + 2)) = calculateCRC32((uint8_t*)packet, 2);
    SendCMDPacketToBootloader(6);
    ReceiveReplayFromBootloader(1);

    if(NOT_ACKNOWLEDGE == packet[0]){ sReplay = "NACK"; }
    else{ sReplay += String(packet[0], HEX); }

    sReplay.toCharArray(Replay, sReplay.length()+1);
    client.publish("/FOTA/BootReply", Replay, false); 
}

void Send_CMD_Jump_To_Application()
{
    char Replay[16] = {0};
    String sReplay = "";
    packet[0] = 5;
    packet[1] = 0x12;
    *((uint32_t*)((uint8_t*)packet + 2)) = calculateCRC32((uint8_t*)packet, 2);
    SendCMDPacketToBootloader(6);
    ReceiveReplayFromBootloader(1);

    if(JUMP_SECCEDDED == packet[0]){ sReplay += "Jump Success"; }
    else{ sReplay += "Jump Fail"; }

    sReplay.toCharArray(Replay, sReplay.length()+1);
    client.publish("/FOTA/BootReply", Replay, false); 
}

void Send_CMD_Erase_Flash()
{
    char Replay[16] = {0};
    String sReplay = "";
    packet[0] = 5;
    packet[1] = 0x13;
    *((uint32_t*)((uint8_t*)packet + 2)) = calculateCRC32((uint8_t*)packet, 2);
    SendCMDPacketToBootloader(6);
    ReceiveReplayFromBootloader(1);

    if(ERASE_SUCCEDDED == packet[0]){ sReplay += "Erase Success";  }
    else{ sReplay += "Erase Unsuccess"; }

    sReplay.toCharArray(Replay, sReplay.length()+1);
    client.publish("/FOTA/BootReply", Replay, false); 
}

void Send_CMD_Upload_Application()
{
    uint8_t isFailed = 0;
    if (Firebase.ready())
    {
        Serial.println("\nDownload file...\n");
  
        // The file systems for flash and SD/SDMMC can be changed in FirebaseFS.h.
        if (!Firebase.Storage.download(&fbdo, STORAGE_BUCKET_ID /* Firebase Storage bucket id */, "Application.bin" /* path of remote file stored in the bucket */, "/update.bin" /* path to local file */, mem_storage_type_flash /* memory storage type, mem_storage_type_flash and mem_storage_type_sd */, fcsDownloadCallback /* callback function */))
            Serial.println(fbdo.errorReason());

        File file = LittleFS.open("/update.bin", "r");                    
        short addressCounter = 0;                                     
        
        while(file.available())
        {
            char Replay[16] = {0};
            String sReplay = "";
            digitalWrite(2, !digitalRead(2));
            uint8_t ByteCounter = 0;                                         
            
            packet[0] = 74;                                               
            packet[1] = 0x14;                                           
            *(int*)(&(packet[2])) = 0x8008000 + (0x40 * addressCounter);  
            packet[6] = 64;                                 
            
            Serial.printf("Start Address: 0x%X\n", *(int*)(&(packet[2])));
            
            while(ByteCounter < 64)
            {
                packet[7 + ByteCounter] = file.read();
                ByteCounter++;
            }
            *((uint32_t*)((uint8_t*)packet + 71)) = calculateCRC32((uint8_t*)packet, 71);
            
            SendCMDPacketToBootloader(75);
            
            ReceiveReplayFromBootloader(1);

            if(WRITE_SUCCEDDED == packet[0]){ sReplay += "Write Success"; }
            else{ isFailed = 1; break; }

            sReplay.toCharArray(Replay, sReplay.length()+1);
            client.publish("/FOTA/BootReply", Replay, false); 

            addressCounter++;
        }

        if(isFailed){ client.publish("/FOTA/BootReply", "Upload Fail", false); }
        else{ client.publish("/FOTA/BootReply", "Upload Success", false);  }
    }
}


/*************************************************** CRC Caculation *****************************************************/
uint32_t calculateCRC32(const uint8_t *pData, uint8_t pLength)
{
    uint8_t crcPacket[290];
    uint16_t crcPacketCounter = 0;
    for(uint8_t i = 0 ; i < pLength ; i++)
    {
        crcPacket[crcPacketCounter++] = 0x00;
        crcPacket[crcPacketCounter++] = 0x00;
        crcPacket[crcPacketCounter++] = 0x00;
        crcPacket[crcPacketCounter++] = pData[i];
    }
    return CRC32_MPEG2(crcPacket, crcPacketCounter);
}

static const uint32_t crc_table[0x100] = {
        0x00000000, 0x04C11DB7, 0x09823B6E, 0x0D4326D9, 0x130476DC, 0x17C56B6B, 0x1A864DB2, 0x1E475005, 0x2608EDB8, 0x22C9F00F, 0x2F8AD6D6, 0x2B4BCB61, 0x350C9B64, 0x31CD86D3, 0x3C8EA00A, 0x384FBDBD,
        0x4C11DB70, 0x48D0C6C7, 0x4593E01E, 0x4152FDA9, 0x5F15ADAC, 0x5BD4B01B, 0x569796C2, 0x52568B75, 0x6A1936C8, 0x6ED82B7F, 0x639B0DA6, 0x675A1011, 0x791D4014, 0x7DDC5DA3, 0x709F7B7A, 0x745E66CD,
        0x9823B6E0, 0x9CE2AB57, 0x91A18D8E, 0x95609039, 0x8B27C03C, 0x8FE6DD8B, 0x82A5FB52, 0x8664E6E5, 0xBE2B5B58, 0xBAEA46EF, 0xB7A96036, 0xB3687D81, 0xAD2F2D84, 0xA9EE3033, 0xA4AD16EA, 0xA06C0B5D,
        0xD4326D90, 0xD0F37027, 0xDDB056FE, 0xD9714B49, 0xC7361B4C, 0xC3F706FB, 0xCEB42022, 0xCA753D95, 0xF23A8028, 0xF6FB9D9F, 0xFBB8BB46, 0xFF79A6F1, 0xE13EF6F4, 0xE5FFEB43, 0xE8BCCD9A, 0xEC7DD02D,
        0x34867077, 0x30476DC0, 0x3D044B19, 0x39C556AE, 0x278206AB, 0x23431B1C, 0x2E003DC5, 0x2AC12072, 0x128E9DCF, 0x164F8078, 0x1B0CA6A1, 0x1FCDBB16, 0x018AEB13, 0x054BF6A4, 0x0808D07D, 0x0CC9CDCA,
        0x7897AB07, 0x7C56B6B0, 0x71159069, 0x75D48DDE, 0x6B93DDDB, 0x6F52C06C, 0x6211E6B5, 0x66D0FB02, 0x5E9F46BF, 0x5A5E5B08, 0x571D7DD1, 0x53DC6066, 0x4D9B3063, 0x495A2DD4, 0x44190B0D, 0x40D816BA,
        0xACA5C697, 0xA864DB20, 0xA527FDF9, 0xA1E6E04E, 0xBFA1B04B, 0xBB60ADFC, 0xB6238B25, 0xB2E29692, 0x8AAD2B2F, 0x8E6C3698, 0x832F1041, 0x87EE0DF6, 0x99A95DF3, 0x9D684044, 0x902B669D, 0x94EA7B2A,
        0xE0B41DE7, 0xE4750050, 0xE9362689, 0xEDF73B3E, 0xF3B06B3B, 0xF771768C, 0xFA325055, 0xFEF34DE2, 0xC6BCF05F, 0xC27DEDE8, 0xCF3ECB31, 0xCBFFD686, 0xD5B88683, 0xD1799B34, 0xDC3ABDED, 0xD8FBA05A,
        0x690CE0EE, 0x6DCDFD59, 0x608EDB80, 0x644FC637, 0x7A089632, 0x7EC98B85, 0x738AAD5C, 0x774BB0EB, 0x4F040D56, 0x4BC510E1, 0x46863638, 0x42472B8F, 0x5C007B8A, 0x58C1663D, 0x558240E4, 0x51435D53,
        0x251D3B9E, 0x21DC2629, 0x2C9F00F0, 0x285E1D47, 0x36194D42, 0x32D850F5, 0x3F9B762C, 0x3B5A6B9B, 0x0315D626, 0x07D4CB91, 0x0A97ED48, 0x0E56F0FF, 0x1011A0FA, 0x14D0BD4D, 0x19939B94, 0x1D528623,
        0xF12F560E, 0xF5EE4BB9, 0xF8AD6D60, 0xFC6C70D7, 0xE22B20D2, 0xE6EA3D65, 0xEBA91BBC, 0xEF68060B, 0xD727BBB6, 0xD3E6A601, 0xDEA580D8, 0xDA649D6F, 0xC423CD6A, 0xC0E2D0DD, 0xCDA1F604, 0xC960EBB3,
        0xBD3E8D7E, 0xB9FF90C9, 0xB4BCB610, 0xB07DABA7, 0xAE3AFBA2, 0xAAFBE615, 0xA7B8C0CC, 0xA379DD7B, 0x9B3660C6, 0x9FF77D71, 0x92B45BA8, 0x9675461F, 0x8832161A, 0x8CF30BAD, 0x81B02D74, 0x857130C3,
        0x5D8A9099, 0x594B8D2E, 0x5408ABF7, 0x50C9B640, 0x4E8EE645, 0x4A4FFBF2, 0x470CDD2B, 0x43CDC09C, 0x7B827D21, 0x7F436096, 0x7200464F, 0x76C15BF8, 0x68860BFD, 0x6C47164A, 0x61043093, 0x65C52D24,
        0x119B4BE9, 0x155A565E, 0x18197087, 0x1CD86D30, 0x029F3D35, 0x065E2082, 0x0B1D065B, 0x0FDC1BEC, 0x3793A651, 0x3352BBE6, 0x3E119D3F, 0x3AD08088, 0x2497D08D, 0x2056CD3A, 0x2D15EBE3, 0x29D4F654,
        0xC5A92679, 0xC1683BCE, 0xCC2B1D17, 0xC8EA00A0, 0xD6AD50A5, 0xD26C4D12, 0xDF2F6BCB, 0xDBEE767C, 0xE3A1CBC1, 0xE760D676, 0xEA23F0AF, 0xEEE2ED18, 0xF0A5BD1D, 0xF464A0AA, 0xF9278673, 0xFDE69BC4,
        0x89B8FD09, 0x8D79E0BE, 0x803AC667, 0x84FBDBD0, 0x9ABC8BD5, 0x9E7D9662, 0x933EB0BB, 0x97FFAD0C, 0xAFB010B1, 0xAB710D06, 0xA6322BDF, 0xA2F33668, 0xBCB4666D, 0xB8757BDA, 0xB5365D03, 0xB1F740B4,
};                        /* Global 32-bit CRC (MPEG-2) Lookup Table */

uint32_t CRC32_MPEG2(uint8_t *p_data, uint32_t data_length)
{
    uint32_t checksum = 0xFFFFFFFF;

    if (data_length == 0xFFFFFFFF)
        return checksum;

    for (unsigned int i = 0 ; i < data_length ; i++)
    {
        uint8_t top = (uint8_t) (checksum >> 24);
        top ^= p_data[i];
        checksum = (checksum << 8) ^ crc_table[top];
    }

    return checksum;
}
