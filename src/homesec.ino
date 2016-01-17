#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <Time.h>
#include <Dns.h>

// number of analog samples to take per reading
#define NUM_SAMPLES 10

int sum = 0;                    // sum of samples taken
unsigned char sample_count = 0; // current sample number
float voltage = 0.0;            // calculated voltage
/* ******** Ethernet Card Settings ******** */
// Set this to your Ethernet Card Mac Address
// THIS IS FAKE!!!
byte mac[] = { 0x90, 0xA2, 0xDA, 0x00, 0x23, 0x36 };

/* ******** NTP Server Settings ******** */
/* us.pool.ntp.org NTP server
   (Set to your time server of choice) */
IPAddress timeServer(216, 23, 247, 62);

/* Set this to the offset (in seconds) to your local time
   This example is GMT - 6 = CST.
   TODO: Doesn't account for daylight savings!!*/
const long timeZoneOffset = -21600L;

/* Syncs to NTP server every 15 seconds for testing,
   set to 1 hour or more to be reasonable */
unsigned int ntpSyncTime = 3600;


/* ALTER THESE VARIABLES AT YOUR OWN RISK */
// local port to listen for UDP packets
unsigned int localPort = 8888;
// NTP time stamp is in the first 48 bytes of the message
const int NTP_PACKET_SIZE= 48;
// Buffer to hold incoming and outgoing packets
byte packetBuffer[NTP_PACKET_SIZE];
// A UDP instance to let us send and receive packets over UDP
EthernetUDP Udp;
// Keeps track of how long ago we updated the NTP server
unsigned long ntpLastUpdate = 0;
// Check last time clock displayed (Not in Production)
time_t prevDisplay = 0;

void setup()
{
    Serial.begin(9600);

    // Ethernet shield and NTP setup
    int i = 0;
    int DHCP = 0;
    DHCP = Ethernet.begin(mac);
    //Try to get dhcp settings 30 times before giving up
    while( DHCP == 0 && i < 30){
        delay(1000);
        DHCP = Ethernet.begin(mac);
        i++;
    }
    if(!DHCP){
        Serial.println("DHCP FAILED");
        for(;;); //Infinite loop because DHCP Failed
    }
    Serial.println("DHCP Success");

    // print your local IP address:
    Serial.print("My IP address: ");
    for (byte thisByte = 0; thisByte < 4; thisByte++) {
        // print the value of each byte of the IP address:
        Serial.print(Ethernet.localIP()[thisByte], DEC);
        Serial.print(".");
    }
    Serial.println();
    IPAddress testIP;

    DNSClient dns;
    // TODO check this
    dns.begin(Ethernet.dnsServerIP());
    dns.getHostByName("pool.ntp.org",testIP);
    Serial.print("NTP IP from the pool: ");
    Serial.println(testIP);
    timeServer = testIP;

    //Try to get the date and time
    int tryAttempts=20;
    int trys=0;
    while(!getTimeAndDate() && trys<tryAttempts) {
        trys++;
    }
    if(trys<tryAttempts){
        Serial.println("ntp server update success");
    }
    else{
        Serial.println("ntp server update failed");
    }
}

// Do not alter this function, it is used by the system
int getTimeAndDate() {
    int flag=0;
    Udp.begin(localPort);
    sendNTPpacket(timeServer);
    delay(1000);
    if (Udp.parsePacket()){
        Udp.read(packetBuffer,NTP_PACKET_SIZE);  // read the packet into the buffer
        unsigned long highWord, lowWord, epoch;
        highWord = word(packetBuffer[40], packetBuffer[41]);
        lowWord = word(packetBuffer[42], packetBuffer[43]);
        epoch = highWord << 16 | lowWord;
        epoch = epoch - 2208988800 + timeZoneOffset;
        flag=1;
        setTime(epoch);
        ntpLastUpdate = now();
    }
    return flag;
}

// Do not alter this function, it is used by the system
unsigned long sendNTPpacket(IPAddress& address)
{
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    packetBuffer[0] = 0b11100011;
    packetBuffer[1] = 0;
    packetBuffer[2] = 6;
    packetBuffer[3] = 0xEC;
    packetBuffer[12]  = 49;
    packetBuffer[13]  = 0x4E;
    packetBuffer[14]  = 49;
    packetBuffer[15]  = 52;
    Udp.beginPacket(address, 123);
    Udp.write(packetBuffer,NTP_PACKET_SIZE);
    Udp.endPacket();
}

// Utility function for clock display: prints preceding colon and leading 0
void printDigits(int digits){
    Serial.print(":");
    if(digits < 10)
        Serial.print('0');
    Serial.print(digits);
}

// Clock display of the time and date (Basic)
void clockDisplay(){
    Serial.print(hour());
    printDigits(minute());
    printDigits(second());
    Serial.print(" ");
    Serial.print(day());
    Serial.print(" ");
    Serial.print(month());
    Serial.print(" ");
    Serial.print(year());
    Serial.println();
}

void loop()
{
    // Update the time via NTP server as often as the time you set at the top
    if(now()-ntpLastUpdate > ntpSyncTime) {
        int trys=0;
        while(!getTimeAndDate() && trys<10){
            trys++;
        }
        if(trys>=10){
            Serial.println("ntp server update still failing!");
            delay(5);
            return;
        }
    }

    // take a number of analog samples and add them up
    while (sample_count < NUM_SAMPLES) {
        sum += analogRead(A5);
        sample_count++;
        delay(10);
    }
    // calculate the voltage
    // use 5.0 for a 5.0V ADC reference voltage
    // 5.07V is the calibrated reference voltage
    float voltage = ((float) sum / (float) NUM_SAMPLES * 5.07) / 1024.0;
    // send voltage for display on Serial Monitor
    // voltage multiplied by 11 when using voltage divider that
    // divides by 11. 11.132 is the calibrated voltage divide
    // value
    clockDisplay();
    Serial.println(voltage * 11.0);
//    Serial.print(voltage * 11.132);
//    Serial.println (" V");
    sample_count = 0;
    sum = 0;
}
