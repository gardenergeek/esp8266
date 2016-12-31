#ifndef UDPLOGGER_H
#define UDPLOGGER_H

namespace util
{	
	class UDPLogger
	{
		
	private:
		bool m_weInitializedWifi;
		in_addr_t m_serverAdress;
		int m_port;
		const char *m_ssid;
		const char *m_pwd;
		
	public:
		UDPLogger(const char *ssid,const char *pwd, in_addr_t serverAdress,int port);	
		UDPLogger(in_addr_t serverAdress,int port);
		
		void log(const char *message);
		void logf(char const *fmt, ...);
		
		~UDPLogger();
	private:
		friend void __threadFunc(void *instance);
	};
	
	extern void __threadFunc(void *instance);
}
#endif