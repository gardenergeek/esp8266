
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

void waitUntilIfReady()
{
	while(wifi_station_get_connect_status() != STATION_GOT_IP);
}	

void initWifiDhcp()
{
	struct station_config * config = (struct station_config *)zalloc(sizeof(struct station_config)); 	
	
	strcpy((char *)config->ssid, "dlink");	
	strcpy((char *)config->password, "tannetorpet107");		
	
	wifi_station_set_config(config);
	free(config);
	wifi_station_connect();
	
	waitUntilIfReady();
}

struct server_cb
{
	int socket;
};

bool readline(int socket,char *buf,int bufflen)
{
	int idx = 0;
	char c;
	
	while(true)
	{
		int res = lwip_recv(socket,&c,1,0);
		
		if(res != 1)
			return false;
		
		// Buffer overflow ..
		if(idx == bufflen -2)
			return false;
		
		
		if(c == '\r')
			continue;
		
		// Eol ?
		if(c == '\n')
		{
			// Yes, null terminate and return
			buf[idx] = 0;
			return true;
		}
		
		// Copy to buffer
		buf[idx++] = c;
	}
}
void print(int socket,const char *s)
{
	lwip_send(socket, s, strlen(s), 0);	
}
void println(int socket,const char *s)
{
	lwip_send(socket, s, strlen(s), 0);	
	lwip_send(socket, "\r\n", 2, 0);
}

void server(void *params)
{
	char rambuf[100];
	rambuf[0] = 0;
	
	
	server_cb *cb = (server_cb*)params;
	
	bool done = false;
	
	do
	{
		print(cb->socket,">");
		if(!readline(cb->socket,rambuf,100))
		{
			done = true;
		}
		else
		{
			if(strlen(rambuf) > 0)
			{
				printf("'%s' (%d)",rambuf,strlen(rambuf));
					
				if(strcmp("q",rambuf) == 0)
				{
					done = true;					
				}
				else
				{
					
					println(cb->socket, "whut?");
				}
			}
		}
	}
	while(!done);
	
	lwip_close(cb->socket);
	
	delete cb;
	printf("Exiting\n");	
	vTaskDelete( NULL );
}

void helloTask(void *pvParameters)
{
	initWifiDhcp();
	
	int socket = lwip_socket(AF_INET, SOCK_STREAM, 0);
	
	sockaddr_in localAddress     = {0};
	
	localAddress.sin_family      = AF_INET;
	localAddress.sin_len         = sizeof(sockaddr_in);
	localAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	localAddress.sin_port        = htons(60000);
	lwip_bind(socket, (sockaddr*)&localAddress, sizeof(sockaddr_in));			


	lwip_listen(socket,5);
	
	
	while(true)
	{
		sockaddr_in remote;
		socklen_t length;
		
		printf("listening\n");
		int s = lwip_accept(socket,(sockaddr*)&remote,&length);
		
		printf("Got connection,spawning thread");
		
		server_cb *cb = new server_cb();
		cb->socket = s;
		
		xTaskHandle t;
		xTaskCreate(server, (const signed char *)"server", 256, cb, 2, &t);
		
		
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
