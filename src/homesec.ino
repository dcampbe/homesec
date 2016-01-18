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
// This is a fake MAC address due to old Ethernet Shield
byte mac[] = { 0x90, 0xA2, 0xDA, 0x00, 0x23, 0x36 };

/* ******** NTP Server Settings ******** */
/* us.pool.ntp.org NTP server
   (Set to your time server of choice) */
IPAddress timeServer(216, 23, 247, 62);

/* Set this to the offset (in seconds) to your local time
   This example is GMT - 6 = CST.
   TODO: Doesn't account for daylight savings!!*/
const long timeZoneOffset = -21600L;

// Syncs to NTP server every hour
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

// Initialize the Ethernet server library
// with the IP address and port you want to use
// (port 80 is default for HTTP):
EthernetServer server(80);

void setup()
{
    Serial.begin(9600);

    // Ethernet shield and NTP setup
    int i = 0;
    int dhcp_rc = 0;
    dhcp_rc = Ethernet.begin(mac);
    //Try to get dhcp settings 30 times before giving up
    while( dhcp_rc == 0 && i < 30){
        delay(1000);
        dhcp_rc = Ethernet.begin(mac);
        i++;
    }
    if(!dhcp_rc){
        Serial.println("Error: DHCP setup failed");
        for(;;); //Infinite loop because DHCP Failed
    }
    Serial.println("DHCP success");

    // print your local IP address:
    Serial.print("My IP address: ");
    for (byte thisByte = 0; thisByte < 4; thisByte++) {
        // print the value of each byte of the IP address:
        Serial.print(Ethernet.localIP()[thisByte], DEC);
        Serial.print(".");
    }
    Serial.println();
    IPAddress timeServer;

    DNSClient dns;
    dns.begin(Ethernet.dnsServerIP());
    if (!dns.getHostByName("pool.ntp.org", timeServer)){
        Serial.println("Error: Could not resolve IP of pool.ntp.org");
        for(;;);
    }
    Serial.print("NTP IP from the pool: ");
    Serial.println(timeServer);

    //Try to get the date and time
    int tryAttempts=20;
    int trys=0;
    while(!getTimeAndDate() && trys<tryAttempts) {
        trys++;
    }
    if(trys==tryAttempts){
        Serial.println("Error: Could not get NTP updates");
        // TODO FIX THIS!
        //for(;;);
    }
    Serial.println("NTP update success");

    //start the server
    server.begin();
    Serial.println("Web server started");
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

void printTime(){
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
        if(trys==10){
            Serial.println("Error: NTP update failed");
        }
    }

    // listen for incoming clients
    EthernetClient client = server.available();
    if (client) {
        Serial.println("new client connected!");
        // an http request ends with a blank line
        boolean currentLineIsBlank = true;
        while (client.connected()) {
            if (client.available()) {
                char c = client.read();
                Serial.write(c);
                // if you've gotten to the end of the line (received a newline
                // character) and the line is blank, the http request has ended,
                // so you can send a reply
                if (c == '\n' && currentLineIsBlank) {
                    // send a standard http response header
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: text/html");
                    client.println(
                            "Connection: close");  // the connection will be closed after completion of the response
                    client.println("Refresh: 5");  // refresh the page automatically every 5 sec
                    client.println();
                    client.println("<!DOCTYPE HTML>");
                    client.println("<html>");
                    // output the value of each analog input pin
                    for (int analogChannel = 0; analogChannel < 6; analogChannel++) {
                        // take a number of analog samples and add them up
                        float reading = analogRead(analogChannel);
                        // 5.07V is the calibrated reference voltage by measuring 5V pins on arduino
                        //voltage is divided by 11. Should be calibrated, but we are looking for tolerance anyways.
                        float voltage = ((reading * 5.07) / 1024.0) * 11.0;
                        String message = "analog reading uninitialized";
                        String color = "black";

                        switch (analogChannel) {
                            case 0:
                                color = "green";
                                message = "Front/Back Door + Living/Kitchen/Bath Windows are secure";
                                if (7.0 <= voltage && voltage < 7.5){
                                    color = "red";
                                    message = "Living, Kitchen, or Bath window is open!";
                                } else if (9.0 <= voltage && voltage < 9.5){
                                    color = "red";
                                    message = "Front or Back door is open!";
                                } else if (13.0 <= voltage && voltage < 14.0){
                                    color = "red";
                                    message = "Front or Back Door and Living, Kitchen, or Bath window is open!";
                                }
                                break;
                            case 1:
                                color = "green";
                                message = "Downstairs window is secure";
                                if (9.0 <= voltage && voltage < 9.5){
                                    color = "red";
                                    message = "Downstairs window is open!";
                                }
                                break;
                            case 2:
                                color = "green";
                                message = "Garage + Bedroom doors are secure";
                                if (7.0 <= voltage && voltage < 7.5){
                                    color = "red";
                                    message = "Garage door is open!";
                                } else if (9.0 <= voltage && voltage < 9.5){
                                    color = "red";
                                    message = "Bedroom door is open!";
                                } else if (13.0 <= voltage && voltage < 14.0){
                                    color = "red";
                                    message = "Garage and bedroom doors are open!";
                                }
                                break;
                            case 3:
                                color = "green";
                                message = "Guest windows are secure";
                                if (9.0 <= voltage && voltage < 9.5){
                                    color = "red";
                                    message = "Guest windows are open!";
                                }
                                break;
                            case 4:
                                color = "green";
                                message = "Balcony door is secure";
                                if (9.0 <= voltage && voltage < 9.5){
                                    color = "red";
                                    message = "Balcony door is open!";
                                }
                                break;
                            case 5:
                                color = "green";
                                message = "Staircase window (2nd floor) is secure";
                                if (9.0 <= voltage && voltage < 9.5){
                                    color = "red";
                                    message = "Staircase window (2nd floor) is open!";
                                }
                                break;
                        }
                        //printTime();
                        client.print("<font color=\"" + color + "\">");
                        client.print(message);
                        client.print("</font>");
                        client.println("<br />");
                    }
                    client.println("</html>");
                    break;
                }
                if (c == '\n') {
                    // you're starting a new line
                    currentLineIsBlank = true;
                } else if (c != '\r') {
                    // you've gotten a character on the current line
                    currentLineIsBlank = false;
                }
            }
        }
        // give the web browser time to receive the data
        delay(1);
        // close the connection:
        client.stop();
        Serial.println("client disconnected");
    }
}
