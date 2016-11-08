
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
// Fel detektering
// konvertering
// Checksumma m.m.

#define BUFFLEN 50
class Dht22
{
	
private:
	core::GPIO m_gpio;
	int m_numIrqs;
//	xQueueHandle m_q;
	
	bool m_lastFlankPositive;
	uint32 m_positiveFlankStart;
	
	
	unsigned char m_buffer[BUFFLEN];
	int m_buffIdx;
	bool m_buffOverflow;
	
public:
	Dht22(int pinNumber):m_gpio(pinNumber)
	{
		m_numIrqs = 0;
		m_buffIdx =0;
//		m_q = NULL;
	}
	bool configure()
	{
/*		m_q = xQueueCreate(41,sizeof(unsigned char));
		
		if(m_q == NULL)
		{
			// TODO error handling
			return false;
		}
*/			
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
		
		// Clear queue and irq state
		clearIrqState();
		
		uint32 result = 0;
		
		while(true)
		{
			printf("Waiting");
			vTaskDelay(5000 / portTICK_RATE_MS);
			
			for(int i = 0; i < m_buffIdx;i++)
			{
				printf("[%d] %d\n",i +1,m_buffer[i]);
			}
		}
	/*	
		
		printf("Unqueueing");
		for(int i = 0; i < 41;i++)
		{
			xQueueReceive(m_q,&result,1000);
			printf("(%d) [%d] %d\n",m_numIrqs, i +1,result);
		}
*/		
		vTaskDelay(5000 / portTICK_RATE_MS);
		
		*humidity = m_numIrqs;
		
		return true;
	}
	
private:
	void clearIrqState()
	{
		m_lastFlankPositive = false;
		m_buffIdx = 0;
		m_buffOverflow = false;
//		xQueueReset(m_q);
	}
	void wakeSensor()
	{
		m_gpio.modeOutput();	
		m_gpio.write(false);	
		os_delay_us(20000);
		
		m_gpio.write(true);	// Issue "go" to sensor		
		os_delay_us(40);	
	}
    void busyWait(uint32 micros)
	{
		uint32 startTs = WDEV_NOW();
		
		while( (WDEV_NOW() - startTs) < micros);
	}
	void isr()
	{
		if(m_gpio.read())
		{
			m_lastFlankPositive = true;
			m_positiveFlankStart = WDEV_NOW();
		}
		else
		{
			m_lastFlankPositive = false;
			uint32 t = (WDEV_NOW() - m_positiveFlankStart);
			
			if(m_buffIdx < BUFFLEN && t < 256)
			{
				m_buffer[m_buffIdx++] = t;
			}
			else
			{
				// Sorry, crap out ..
				m_buffOverflow = true;
			}
			// 
			// We borrow to calculated
//			m_positiveFlankStart =(WDEV_NOW() - m_positiveFlankStart);
//			portBASE_TYPE woken;
			// We have a negative flank .. let them now
//			portBASE_TYPE result = xQueueSendFromISR(m_q,&m_positiveFlankStart,&woken);
		}
		
		
		m_numIrqs++;
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
		printf("Success %d\n",hum);			
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
