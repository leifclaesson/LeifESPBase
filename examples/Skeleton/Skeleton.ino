#include <LeifESPBase.h>

// *** Don't forget to add your WiFi SSID/Key to LeifESPBase\environment_setup.h See environment_setup.h.example for details. ***


//The code keeps track of whether it's running on a production unit
//(let's say inside a ceiling-mounted box, inconvenient to get to)
//or a development board on your bench, by way of checking the MAC address.

//This reduces confusion while bench-testing new code before OTA upload to the production unit.
//The bDeployedUnit flag is set at the beginning of the setup() function below.

bool bDeployedUnit=false;

const char * GetHostName()	//this host name will reported to for example mDNS, telnet
{
	if(bDeployedUnit)
	{
		return "BathroomMCU";
	}
	return "BathroomMCU-dev";
}

const char * GetHeadingText()	//friendly system name, used for example for the HTTP page
{
	if(bDeployedUnit)
	{
		return "Bathroom MCU";
	}
	else
	{
		return "Bathroom MCU dev";
	}
}



//this function will be called by all the URL handlers.
//I truly suck at webdesign. You can do better.
void genHtmlPage(String & output, const String & strInsert)
{

	output.reserve(2048);

	output.concat("<!DOCTYPE html>");
	output.concat("<html><head><style>table, th, td {  border: 1px solid black;  border-collapse: collapse;}th, td {  padding: 5px;}</style></head>");
	output.concat("<body>");
	output.concat("<h2>");
	output.concat(GetHeadingText());
	output.concat("</h2>");

	LeifHtmlMainPageCommonHeader(output);

	output.concat("<table><tr>");

	output.concat("<td><a href=\"/\">Reload</a></td>");
	output.concat("<td><a href=\"/ping\">Minimal test page</a></td>");
	output.concat("<td><a href=\"/sysinfo\">System Info</a></td>");
	output.concat("<td><a href=\"/invert\">Invert status LED</a></td>");
	output.concat("</tr></table>");

	output.concat(strInsert);

	output.concat("</body></html>");
}


void handleRoot()
{
	String strInsert;

	String strPage;
	genHtmlPage(strPage,strInsert);

	server.send(200, "text/html", strPage);
}


void handleInvert()
{

	LeifSetInvertLedBlink(!LeifGetInvertLedBlink());	//controls whether the LED is normally off or normally on.


	String strInsert;

	strInsert="<p>Status LED pattern inverted. Now normally ";
	strInsert+=LeifGetInvertLedBlink()?"OFF":"ON";
	strInsert+=".</p>";

	String strPage;
	genHtmlPage(strPage,strInsert);

	server.send(200, "text/html", strPage);
}

void handleNotFound()
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}




void setup()
{
	//MAC address of your deployed (production) unit
	if(WiFi.macAddress()=="BC:DD:C2:23:68:54") bDeployedUnit=true;

	//The LED is used for status indication. Flashes twice a second while connecting WiFi.
	//Flashes once every two seconds when connected.

	//By default, LED_BUILTIN is used for status output. 2 Hz flashing while connecting to WiFi, 0.5 Hz when connected.
	//You can use any pin by calling the LedSetStatusLedPin(x) function like below. -1 disables output altogether.
	//LeifSetStatusLedPin(-1);	//No status LED output
	//LeifSetStatusLedPin(1);	//ESP-01 on-board LED. This is actually the serial port so it will disable serial console output!

	LeifSetupBegin();	//starts initialization of the HTTP server and other objects.
						//Set your serial console to 115200 bps to see console output. The console will print the IP address and many other things.
						//Once you know the IP address you can connect to port 23 with a telnet client like PuTTY to see console output.
						//You'll need to enable the "Implicit CR option in every LF" on the Terminal Page in PuTTY.

	server.onNotFound(handleNotFound);	//serve 404
	server.on("/", handleRoot);		//serve root page, defined above.

	//define your own URL handlers here.
	server.on("/invert", handleInvert);		//invert the LED flashing pattern.


	LeifSetupEnd();	//finishes initialization of the HTTP server and other objects
}

void loop()
{
	LeifLoop();

	//Automatically reboot if we're unable to ping the gateway for 5 minutes. ESP8266 only for now!
	//LeifGatewayKeepalive();	//uncomment if desired


	if(Interval100())
	{
		//True once every 100ms. Just for convenience.
	}

	if(Interval250())
	{
		//True once every 250ms. Just for convenience.
	}

	if(Interval1000())
	{
		//True once every second. Just for convenience.
	}

	if(Interval10s())
	{
		//True once every second. Just for convenience.
	}

	if(Interval1000() && (seconds()%60)==0)	//once a minute
	{
		String strUptime;
		LeifUptimeString(strUptime);
		//print a status message to the serial console and the telnet console.
		//you can disable the serial console by compiling with NO_SERIAL_DEBUG
		csprintf("\nUptime=%s ESP.getFreeHeap()=%u Seconds=%lu  WiFi signal: %i\n",strUptime.c_str(),ESP.getFreeHeap(),seconds(),WiFi.RSSI());
	}

}
