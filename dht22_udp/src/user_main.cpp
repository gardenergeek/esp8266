
extern "C"
{
#include "esp_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

	#include "lwip/sockets.h"
extern void uart_div_modify(int,int);

}

int32_t getLastError(int socket)
{
	int32_t r = 0;
	socklen_t l = sizeof(int32_t);
	lwip_getsockopt(socket, SOL_SOCKET, SO_ERROR, (void *)&r, &l);
	return r;
}

#include "dht.h"
driver::Dht22 dht22(4);
	
void helloTask(void *pvParameters)
{
	printf("Hello\n");	
	
	struct station_config * config = (struct station_config *)zalloc(sizeof(struct station_config)); 	
	
	strcpy((char *)config->ssid, "dlink");	
	strcpy((char *)config->password, "tannetorpet107");		
	
	wifi_station_set_config(config);
	free(config);
	wifi_station_connect();
	
	sockaddr_in m_remoteAddress;    // used by sendTo
		
	int socket = lwip_socket(AF_INET, SOCK_DGRAM, 0);
	
	sockaddr_in localAddress     = {0};
	
	localAddress.sin_family      = AF_INET;
	localAddress.sin_len         = sizeof(sockaddr_in);
	localAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	localAddress.sin_port        = htons(60000);
	lwip_bind(socket, (sockaddr*)&localAddress, sizeof(sockaddr_in));			
	
	// Addr
	in_addr_t a;

	((uint8_t*)&a)[0] = 192;
	((uint8_t*)&a)[1] = 168;
	((uint8_t*)&a)[2] = 0;
	((uint8_t*)&a)[3] = 105;
	
	
	
	memset(&m_remoteAddress, 0, sizeof(sockaddr_in));
	m_remoteAddress.sin_family      = AF_INET;
	m_remoteAddress.sin_len         = sizeof(sockaddr_in);
	m_remoteAddress.sin_addr.s_addr = a;
	m_remoteAddress.sin_port        = htons(2000);
	
	char rambuf[100];
	memcpy(rambuf, "Hej",3);
	

	dht22.configure();
	int temp;
	int hum;
	int len = 0;
	
	while(true)
	{
		printf("Waiting\n");
		vTaskDelay(5000 / portTICK_RATE_MS);		
		
		if(dht22.read(&hum,&temp))
		{
			printf("Success ");	
			
			printf("Humidity: %d %% Temperature: %dC\n",hum,temp);
			sprintf(rambuf,"Humidity: %d %% Temperature: %dC\n",hum,temp);
			len = strlen(rambuf);
		}
		else
		{
			printf("Failed %s\n",dht22.getLastResultMessage());
		}
		
		printf("Sending\n");
		lwip_sendto(socket, rambuf, len, 0, (sockaddr*)&m_remoteAddress, sizeof(m_remoteAddress));		
		
		printf("Result:%u\n",getLastError(socket));
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
		wifi_set_opmode(STATION_MODE);	
		
		xTaskHandle t;
		ret = xTaskCreate(helloTask, (const signed char *)"rx", 256, NULL, 2, &t);
		printf("xTaskCreate returns %d handle is %d\n", ret, t);
	}
}
