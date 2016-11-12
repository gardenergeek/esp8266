
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


//core::GPIO gpio(4);

//
// TODO: Snygga till
// Checksumma
// signbit temp.
// timing
/*
class BitSet16
{
private:	
	uint16 _bits;
	
public:	
	BitSet16()
	{
		_bits = 0;
	}
	BitSet16(uint16 bits)
	{
		_bits = bits;
	}
	void Clear()
	{
		_bits = 0;
	}
	void SetBit(int nr,bool value)
	{
		nr = nr % 16;
		
		_bits != 0x0001<<nr;
	}
	bool GetBit(int nr)
	{
		nr = nr % 16;
		
		return (_bits & 0x0001<<nr) > 0;
	}
	
	uint16 GetValue()
	{
		return _bits;
	}
	unsigned char GetValue8()
	{
		return (unsigned char)(0x00ff & _bits);
	}
};
*/
typedef enum {
	DHTOK = 0,				// Evrything fine
	DHTERROR_BUSHANG = 1,  // No response
	DHTERROR_TREH_INVALID = 2, // Invalid timing of read values
	DHTERROR_DATA_INVALID = 3,
	DHTERROR_BUFFEROVERFLOW = 4,
	DHTERROR_CHECKSUM = 5,
} DhtState;

#define DATABITS 40
class Dht22
{
	
private:
	core::GPIO m_gpio;
	
	bool m_lastFlankPositive;
	uint32 m_positiveFlankStart;

	int m_bitIdx;
	bool m_dataBits;
	DhtState m_state;
	
	unsigned char m_dataBuffer[DATABITS];
	
public:
	Dht22(int pinNumber):m_gpio(pinNumber)
	{
		clearReadState();
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
		// Reset buss ..
		wakeSensor();
		
		// .. and release the bus, so the sensor could drive it for data
		m_gpio.modeInput();
		
		// clear our state
		clearReadState();
		
		uint32 result = 0;
		
		printf("Waiting");		
		vTaskDelay(1000 / portTICK_RATE_MS);
		
		if(readingDone())
		{
			*humidity = 0;
			*temperature = 0;
			int checksum = 0;
			// Alright, then process values
			for(int i = 0; i < DATABITS;i++)
			{
				bool v;		
			    unsigned char t = m_dataBuffer[i];
				printf("%d=>",t);	
				// Validate & convert
				if(t >= 50 && t <= 100) // High ?
				{
					// Yep. 
					v = true;
				}
				else if(t >= 22 && t < 50) // Low ?
				{
					// Yep.
					v = false;
				}
				else
				{
					// Crap.
					m_state = DHTERROR_DATA_INVALID;			
					return false;
				}
				printf("%d\n",v?1:0);
				
				if(v)
				{
					if(i < 16)	// 0-15 humidity
					{
						*humidity |= 0x8000>>i;
					}
					else if(i >= 16 && i < 32) // 16-31 Temp
					{
						*temperature |= 0x8000>>(i - 16);
					}
					else	// Check sum
					{
						checksum |= 0x80>>(i - 32);
					}
				}
			}
			
			// Ok, lets check the check sum
			int calculatedChecksum = (*humidity & 0xff00)>>8 + (*humidity & 0x00ff) + (*temperature & 0xff00)>>8 + (*temperature & 0x00ff);
			printf("%d == %d\n",calculatedChecksum,checksum);
			if(calculatedChecksum != checksum)
			{
				*humidity = 0;
				*temperature = 0;
				
				m_state =DHTERROR_CHECKSUM;
				return false;
				
			}
			printf("\n");							
			return true;

			
		}
		else
		{
			// Timeout
		}
		return false;
	}
	
	DhtState getLastResult()
	{
		return m_state;
	}
private:
	bool readingDone()
	{
		return m_bitIdx == DATABITS;
	}
	
	void clearReadState()
	{
		m_lastFlankPositive = false;
		m_positiveFlankStart = 0;
		m_bitIdx = 0;
		m_state = DHTOK;
		m_dataBits = false;
	}
	void wakeSensor()
	{
		// Drive bus low
		m_gpio.modeOutput();	
		m_gpio.write(false);	
		os_delay_us(20000);
//		busyWait(20000);
		m_gpio.write(true);	// Issue "go" to sensor		
//		busyWait(40);
		os_delay_us(40);
	}
    void busyWait(uint32 micros)
	{
		uint32 startTs = WDEV_NOW();
		
		while( (WDEV_NOW() - startTs) < micros);
	}
	void isr()
	{
		if(m_state != DHTOK)
			return;
		
		if(m_gpio.read())
		{
			m_lastFlankPositive = true;
			m_positiveFlankStart = WDEV_NOW();
		}
		else
		{
			// Is this a correct value ..?
			if(m_lastFlankPositive)
			{
				// Yes, we have had a positive flank before
				// have we recive start signal from sensor ..?
				if(!m_dataBits)
				{
					// nope, then this must be Treh
					m_dataBits = true;
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
			
			m_lastFlankPositive = false;
		}
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
		printf("Failed\n");
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
