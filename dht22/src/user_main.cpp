
extern "C"
{
#include "esp_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern void uart_div_modify(int,int);

#define REG_WRITE(_r,_v)    (*(volatile uint32 *)(_r)) = (_v)
#define REG_READ(_r)        (*(volatile uint32 *)(_r))
#define WDEV_NOW()          REG_READ(0x3ff20c00)

}
#define PIN 4

#include "coregpio.h"
core::GPIO gpio(4);

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


/*
 * this task will read the uart and echo back all characters entered
 */
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
