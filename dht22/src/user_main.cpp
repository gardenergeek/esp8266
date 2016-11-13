
extern "C"
{
#include "esp_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

extern void uart_div_modify(int,int);

}

#include "dht.h"
driver::Dht22 dht22(4);

void print(int v)
{
	int i = v/10;
	int f = v % 10;
	
	printf("%d.%d",i,f);
}
void helloTask(void *pvParameters)
{
	printf("Waiting\n");	
	vTaskDelay(5000 / portTICK_RATE_MS);
	printf("configure\n");
	dht22.configure();	
	
	printf("starting to read\n");	
	int temp;
	int hum;

	while(1)
	{
		if(dht22.read(&hum,&temp))
		{
			printf("Success ");	
			print(hum);
			printf("%% ");
			print(temp);
			printf("C\n");
		}
		else
		{
			printf("Failed %s\n",dht22.getLastResultMessage());
		}
		vTaskDelay(5000 / portTICK_RATE_MS);		
	}
}

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
