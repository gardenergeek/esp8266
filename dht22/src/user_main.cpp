
extern "C"
{
#include "esp_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

extern void uart_div_modify(int,int);

#define REG_WRITE(_r,_v)    (*(volatile uint32 *)(_r)) = (_v)
#define REG_READ(_r)        (*(volatile uint32 *)(_r))
#define WDEV_NOW()          REG_READ(0x3ff20c00)

}
#define PIN 4

#include "coregpio.h"


typedef enum {
	DHTOK = 0,				// Evrything fine
	DHTERROR_NORESPONSE = 1,  // No response
	DHTERROR_TIMEOUT = 2,  // Invalid timing of read values
	DHTERROR_CHECKSUM = 3,
	DHTERROR_RESAMPLING = 4, // Must be at least 2s between measurements 
} DhtState;

#define DATABITS 40
class Dht22
{
	
private:
	core::GPIO m_gpio;
	
	bool m_lastValue;
	uint32 m_positiveFlankStart;

	int m_bitIdx;
	bool m_gotResponseHigh;
	DhtState m_state;
	
	unsigned char m_dataBuffer[DATABITS];
	
	uint32 m_lastReadTs;
	
public:
	Dht22(int pinNumber):m_gpio(pinNumber)
	{
		clearReadState();
		m_lastReadTs = 0;
	}
	bool configure()
	{
		// Configure io-pin, we use pullup to keep high, altough an input
		m_gpio.modeInput();
		m_gpio.enablePullUp(true);	// Pull pin high
		
		// Attach irq handler
		m_gpio.attachInterruptHandler(Dht22::sharedIsr,this,core::GPIO_PIN_INTR_ANYEDGE);
		
		return true;
	}
	
	bool read(int *humidity,int *temperature)
	{
		// Check so that at least 2s between samples
		if(m_lastReadTs != 0 && 	// Never read
		  (WDEV_NOW() - m_lastReadTs) < 2000000 &&	// Less than 2s
		  WDEV_NOW() > m_lastReadTs) // Sanity check if RTC flipped around
		{
			m_state = DHTERROR_RESAMPLING;
			return false;
		}
		
		// Reset bus ..
		wakeSensor();
		
		// .. and release the bus, so the sensor could drive it for data
		m_gpio.modeInput();
		
		// clear our state
		clearReadState();
		
		// Reasonable timout, 85+85 + 40*(55+75) + 55 = 170 + 5200 + 55 = 5425microsecons
		// => 5.5ms, we are generous 20ms
		vTaskDelay(20 / portTICK_RATE_MS);
		
		if(readingDone())
		{
			unsigned char hhumidity = toByte(&m_dataBuffer[0]);
			unsigned char lhumidity= toByte(&m_dataBuffer[8]);	
			unsigned char htemp= toByte(&m_dataBuffer[16]);
			unsigned char ltemp= toByte(&m_dataBuffer[24]);	
			unsigned char checksum= toByte(&m_dataBuffer[32]);
			
			unsigned char calculated = (hhumidity + lhumidity + htemp + ltemp) & 0x0FF;
			
			// Ok, lets check the check sum
			if(calculated == checksum)
			{
				unsigned int hum = hhumidity<<8 | lhumidity;
				unsigned int temp = (htemp<<8 | ltemp) & 0x00007ff;	// Mask sign bit
				
				*humidity = hum;
				*temperature = temp*((htemp & 0x80)?-1:1);
				
				// Last read
				m_lastReadTs = WDEV_NOW();				
				return true;
			}
			m_state =DHTERROR_CHECKSUM;
			return false;
		}
		else
		{
			// Check, error so that we could be more specific
			if(m_bitIdx == 0)	// Nothing at all ...
				m_state = DHTERROR_NORESPONSE;
			else
				m_state = DHTERROR_TIMEOUT;
		}
		return false;
	}
	
	DhtState getLastResult()
	{
		return m_state;
	}
	
	const char *getLastResultMessage()
	{
		switch(m_state)
		{
			case DHTOK :
			return "OK";
			
			case DHTERROR_NORESPONSE :
			return "No response";
			
			case DHTERROR_RESAMPLING :
			return "Resampling, wait 2s";
			
			case DHTERROR_CHECKSUM :
			return "CRC error";
			
			case DHTERROR_TIMEOUT :
			return "Timeout error";
		}
		
		return "Unknown error";
	}
private:
	unsigned char toByte(unsigned char *start)
	{
		unsigned char result = 0;
		unsigned char mask = 0x0080;
		
		for(int i = 0; i < 8;i++)
		{
			if(start[i] > 50)
			{
				result |= mask>>i;
			}
		}
		return result;
	}

	bool readingDone()
	{
		return m_bitIdx == DATABITS;
	}
	
	void clearReadState()
	{
		m_lastValue = false;
		m_positiveFlankStart = 0;
		m_bitIdx = 0;
		m_state = DHTOK;
		m_gotResponseHigh = false;
	}
	void wakeSensor()
	{
		// Drive bus low
		m_gpio.modeOutput();	
		
		m_gpio.write(false);	
		busyWait(20000);
		
		m_gpio.write(true);	// Issue "go" to sensor		
		busyWait(40);
	}
    void busyWait(uint32 micros)
	{
		uint32 startTs = WDEV_NOW();
		
		while( (WDEV_NOW() - startTs) < micros);
	}
	void isr()
	{
		bool currentVal = m_gpio.read();
		
		if(!m_lastValue && currentVal)	// Positive flank
		{
			m_positiveFlankStart = WDEV_NOW();			
		}
		else if(m_lastValue && !currentVal)	// Negative flank
		{
			// have we recive start signal from sensor ..?
			if(!m_gotResponseHigh)
			{
				// nope, then this must be responseHigh
				m_gotResponseHigh = true;
			}
			else
			{
				// Data is flowing
				if(m_bitIdx < DATABITS)
				{
					m_dataBuffer[m_bitIdx++] = (WDEV_NOW() - m_positiveFlankStart);
				}
			}			
		}
		m_lastValue = currentVal;
		
	}
	static void sharedIsr(void *instance)
	{
		((Dht22 *)instance)->isr();
	}
};

Dht22 dht22(4);













void helloTask(void *pvParameters)
{
	printf("Waiting\n");	
	vTaskDelay(5000 / portTICK_RATE_MS);
	printf("configure\n");
	dht22.configure();
	
	printf("starting to read\n");	
	int temp;
	int hum;

	if(dht22.read(&hum,&temp))
	{
		printf("Success %d %d\n",hum,temp);	
	}
	else
	{
		printf("Failed %s\n",dht22.getLastResultMessage());
	}
	
	if(dht22.read(&hum,&temp))
	{
		printf("Success %d %d\n",hum,temp);	
	}
	else
	{
		printf("Failed %s\n",dht22.getLastResultMessage());
	}

	while(1);
}









/*

int numints = 0;
uint32 lastTs = 0;

struct flank
{
	bool val;
	uint32 offset;
};
struct flank values[82];
int valueidx = 0;

//flank[82] values;

void myisr()
{
	
	values[valueidx].offset = WDEV_NOW() - lastTs;
	values[valueidx].val = gpio.read();	
	lastTs =WDEV_NOW();
	valueidx++;
	numints++;
}

unsigned char toByte(int *start)
{
	unsigned char result = 0;
	unsigned char mask = 0x0080;
	
	for(int i = 0; i < 8;i++)
	{
		if(start[i] > 50)
		{
			printf("1");			
			result |= mask>>i;
		}
		else
		{
			printf("0");				
		}
	}
	printf("\n");
	
	return result;
}


 
void helloTask(void *pvParameters)
{
	gpio.modeInput();
	gpio.enablePullUp(true);	// Pull pin high

	vTaskDelay(5000 / portTICK_RATE_MS);
	printf("Dropping bus\n");
		
	// Drag bus low
	gpio.modeOutput();	
	gpio.write(false);	
    os_delay_us(20000);
	
	gpio.write(true);	// Issue "go" to sensor		
    os_delay_us(40);	
	
	// .. and release the bus, so the sensor could drive it for data
	gpio.modeInput();
	
	// Wait unit it goes low
	lastTs = WDEV_NOW();
	gpio.attachInterruptHandler(myisr,core::GPIO_PIN_INTR_ANYEDGE);
	
	// Wait for data values from irq-handler
	while(valueidx < 82)
	{
		printf("Waiting for data (%d)\n",valueidx);
		vTaskDelay(1000 / portTICK_RATE_MS);		
	}
	
	printf("Data is ready!");	
	
	
	// Lets print it!
	for(int i = 0; i < 82;i++)
	{
		printf(values[i].val?"+":"-");
		printf("%d\n",values[i].offset);
	}
	
	int bittimes[40];
	
	// And lets "translate"
	int bitidx = 0;
	for(int i = 0; i < 82;i++)
	{
		// Ignore the first, its i ACK
		if(!values[i].val && i > 1)
		{
			bittimes[bitidx++] = values[i].offset;
			printf("%d\n",values[i].offset);			
		}
	}
	for(int i = 0; i < 40;i++)
	{
		printf("[%d] %d\n",i,bittimes[i]);			
	}
	
	unsigned char hhumidity = toByte(&bittimes[0]);
	unsigned char lhumidity= toByte(&bittimes[8]);	
	unsigned char htemp= toByte(&bittimes[16]);
	unsigned char ltemp= toByte(&bittimes[24]);	
	unsigned char check= toByte(&bittimes[32]);
	
	unsigned char calculated = (hhumidity + lhumidity + htemp + ltemp) & 0x0FF;
	
	
	
	
	unsigned int humidity = hhumidity<<8 | lhumidity;
	unsigned int temp = htemp<<8 | ltemp;	
	
	printf("Humidity:%u Temp:%u\n",humidity,temp);				
	printf("checksum:%u = %u\n",check,calculated);	
	while(1);
	
}
*/
//
//
// Snyggab till initiering
//
// Övergång från driv till lyssnande
// Objektifiera


/*
 * This is entry point for user code
 */
extern "C"
{
	void ICACHE_FLASH_ATTR user_init(void)
	{

		portBASE_TYPE ret;
		
		// unsure what the default bit rate is, so set to a known value
		uart_div_modify(0, UART_CLK_FREQ / 115200);
		wifi_set_opmode(NULL_MODE);	
		
		xTaskHandle t;
		ret = xTaskCreate(helloTask, (const signed char *)"rx", 256, NULL, 2, &t);
		printf("xTaskCreate returns %d handle is %d\n", ret, t);
	}
}
