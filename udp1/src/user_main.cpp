
extern "C"
{
#include "esp_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"

#include <stdarg.h>
extern void uart_div_modify(int,int);

}
#define DEBUGLOG(s) printf(s);

namespace util
{	
	class UDPLogger
	{
		
	private:
		bool m_weInitializedWifi;
		sockaddr_in m_remoteAddress;
		int m_port;
		const char *m_serverIp;
		const char *m_ssid;
		const char *m_pwd;
		
		bool m_initialized;
		bool m_wifiInitialized;
		bool m_socketInitialized;
		
		int m_socket;
	public:
		UDPLogger(const char *ssid,const char *pwd, const char *serverIp,int port);	
		UDPLogger(const char *serverIp,int port);
		
		void log(const char *message);
		void logf(char const *fmt, ...);
		
		~UDPLogger();
	private:
		bool initWifi();
		bool initSocket();
		bool init();
		bool parseIp(const char *ip,in_addr_t *addr);
		bool parseInt(char *str,int *v);
		int raisedTo(int x,int y);
		int32_t getLastError();
		bool write(void const* buffer, uint32_t length);
		int32_t min(int32_t a,int32_t b);
		friend void __threadFunc(void *instance);
	};
	
	extern void __threadFunc(void *instance);
	
	UDPLogger::UDPLogger(const char *ssid,const char *pwd, const char *serverIp,int port)
	{
		m_ssid = ssid;
		m_pwd = pwd;
		
		m_serverIp = serverIp;
		m_port = port;
		
		m_initialized = false;
		m_weInitializedWifi = false;
		m_wifiInitialized = false;
		m_socket = 0;
	}
	UDPLogger::UDPLogger(const char *serverIp,int port)
	{
		m_ssid = NULL;
		m_pwd = NULL;
		
		m_serverIp = serverIp;
		m_port = port;
		
		m_initialized = false;
		m_weInitializedWifi = false;
		m_wifiInitialized = true;
		m_socket = 0;		
	}
	void UDPLogger::log(const char *message)
	{
		init();
		if(write(message,strlen(message)))
		{
			printf("OK!\n");
		}
		else
		{
			printf("KUK!\n");
		}
	}
	void UDPLogger::logf(char const *fmt, ...)
	{
		init();
		/*
		
		VRÅL KUKAR, troligtvis minnesfel
        va_list args;
        
        va_start(args, fmt);
        uint16_t len = sprintf(NULL, fmt, args);
        va_end(args);

        char buf[len + 1];
        
        va_start(args, fmt);
        sprintf(buf, fmt, args);
        va_end(args);
        
        write(buf, len);
		*/
	}
		
	UDPLogger::~UDPLogger()
	{
		
	}
	int32_t UDPLogger::min(int32_t a,int32_t b)
	{
		return a < b?a:b;
	}
	
	int32_t UDPLogger::getLastError()
	{
		int32_t r = 0;
		socklen_t l = sizeof(int32_t);
		lwip_getsockopt(m_socket, SOL_SOCKET, SO_ERROR, (void *)&r, &l);
		return r;
	}
	
    bool UDPLogger::write(void const* buffer, uint32_t length)
    {
        static uint32_t const MAXCHUNKSIZE = 128;
        
        int32_t bytesSent = 0;
        // send in chunks of up to MAXCHUNKSIZE bytes
        uint8_t rambuf[min(length, MAXCHUNKSIZE)];
        uint8_t const* src = (uint8_t const*)buffer;
        while (bytesSent < length)
        {
            uint32_t bytesToSend = min(MAXCHUNKSIZE, length - bytesSent);
            memcpy(rambuf, src, bytesToSend);
            int32_t chunkBytesSent = lwip_sendto(m_socket, rambuf, bytesToSend, 0, (sockaddr*)&m_remoteAddress, sizeof(m_remoteAddress));
			
            int32_t lasterr = getLastError();                
            if (lasterr == EAGAIN || lasterr == EWOULDBLOCK)
            {
                chunkBytesSent = 0;
            }
            else if (chunkBytesSent <= 0 || lasterr != 0)
            {
                // error
                bytesSent = -1;
                break;
            }
            bytesSent += chunkBytesSent;
            src += chunkBytesSent;
        }
        return bytesSent > 0;
    }
	
	
	bool UDPLogger::initWifi()
	{
		if(!m_wifiInitialized)
		{
			DEBUGLOG("initWifi\n")
			struct station_config * config = (struct station_config *)zalloc(sizeof(struct station_config)); 	
			
			strcpy((char *)config->ssid, m_ssid);	
			strcpy((char *)config->password, m_pwd);		
			
			wifi_station_set_config(config);
			free(config);
			wifi_station_connect();
			
	//		while(wifi_station_get_connect_status() != STATION_GOT_IP);
			
			m_weInitializedWifi = true;
			
			DEBUGLOG("Wifi initialized\n")		
			m_wifiInitialized = true;
		}
		
		return true;
	}
	bool UDPLogger::initSocket()
	{
		if(!m_socketInitialized)
		{
			DEBUGLOG("initSocket\n")
			
			if(m_socket > 0)
			{
				lwip_close(m_socket);
				m_socket = 0;
			}
			m_socket = lwip_socket(AF_INET, SOCK_DGRAM, 0);
		
			printf("%d\n",m_socket);
			if(m_socket <= 0)
				return false;
			
			DEBUGLOG("socket created\n")		
			sockaddr_in localAddress     = {0};
		
			localAddress.sin_family      = AF_INET;
			localAddress.sin_len         = sizeof(sockaddr_in);
			localAddress.sin_addr.s_addr = htonl(INADDR_ANY);
			localAddress.sin_port        = htons(60000);
			int res =lwip_bind(m_socket, (sockaddr*)&localAddress, sizeof(sockaddr_in));			
			
			printf("socket bound %d\n",res);	
			if(res < 0)
				return false;

			// Addr
			in_addr_t a;
			
			
			if(!parseIp(m_serverIp,&a))
				return false;
		
			DEBUGLOG("ip parsed\n")
			memset(&m_remoteAddress, 0, sizeof(sockaddr_in));
			m_remoteAddress.sin_family      = AF_INET;
			m_remoteAddress.sin_len         = sizeof(sockaddr_in);
			m_remoteAddress.sin_addr.s_addr = a;
			m_remoteAddress.sin_port        = htons(m_port);
			
			DEBUGLOG("Done\n")
			m_socketInitialized = true;
			return true;
		}
	}
	bool UDPLogger::init()
	{
		if(!m_initialized)
		{
			m_initialized = initWifi() && initSocket();
		}
	}
	
	bool UDPLogger::parseIp(const char *ip,in_addr_t *addr)
	{
		char buf[4];
		
		int parts[4];
		int partsIdx = 0;
		int s = strlen(ip);
		int bufidx = 0;
		
		for(int idx = 0;idx < s;idx++ )
		{
			printf("%c [%d]",ip[idx],ip[idx]);
			
			if(ip[idx] == '.')
			{
				// Null terminate and convert
				buf[bufidx] = 0;
				
				printf("Converting:%s",buf);
				
				if(!parseInt(buf,&parts[partsIdx]))
					return false;
				
				printf("\nGot:%d\n",parts[partsIdx]);				
				partsIdx++;
				bufidx = 0;
			}
			else
			{
				buf[bufidx++] = ip[idx];
			}
		}
		
		// Last part ..
		if(partsIdx == 3)
		{
			buf[bufidx] = 0;			
			if(!parseInt(buf,&parts[partsIdx]))
				return false;

			((uint8_t*)addr)[0] = parts[0];
			((uint8_t*)addr)[1] = parts[1];
			((uint8_t*)addr)[2] = parts[2];
			((uint8_t*)addr)[3] = parts[3];
			
			for(int j = 0; j < 4;j++)
			{
				printf("%d ",parts[j]);
			}
			printf("\n");					
			return true;					
		}
		
		return false;
	}
	int UDPLogger::raisedTo(int x,int y)
	{
		if(y <= 0)
			return 1;
		
		int res = x;
		
		for(int i = 1;i < y;i++)
		{
			res = res*x;
		}
		
		return res;
	}
	bool UDPLogger::parseInt(char *str,int *v)
	{
		int len = strlen(str);
		
		printf("length:%d\n",len);
		int startBase = raisedTo(10,len -1);
		
		
		printf("base:%d\n",startBase);		
		*v = 0;
		
		for(int idx = 0; idx < len;idx++)
		{
			if(str[idx] > '9' && str[idx] < '0')
				return false;
			
			int d = str[idx] - '0';
			
			*v += d*startBase;
			
			startBase = startBase/10;
		}
		
		return true;
	}
	
	void __threadFunc(void *instance)
	{
		
	}
}

void helloTask(void *pvParameters)
{
		vTaskDelay(5000 / portTICK_RATE_MS);			
	util::UDPLogger u = util::UDPLogger("dlink","tannetorpet107","192.168.0.106",2000);
	
	while(true)
	{
		printf("Waiting\n");
		vTaskDelay(5000 / portTICK_RATE_MS);		

		u.log("Apa");		
		
		u.logf("Apan: %s => %d","Niklas",10);				
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
