#ifndef DHT_H
#define DHT_H

#include "macros.h"
#include "coregpio.h"

namespace driver
{	

	typedef enum {
		DHTOK = 0,					// Evrything fine
		DHTERROR_NORESPONSE = 1,  	// No response
		DHTERROR_TIMEOUT = 2,  		// Invalid timing of read values
		DHTERROR_CHECKSUM = 3,		// Checksum error
		DHTERROR_RESAMPLING = 4, 	// Must be at least 2s between measurements 
		DHTERROR_NOTCONFIGURED = 5	// Must configure before use
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
		
//		unsigned char m_dataBuffer[DATABITS];
		unsigned char *m_dataBuffer;		
		uint32 m_lastReadTs;
		
	public:
		Dht22(int pinNumber);
		~Dht22();
		bool configure();
		bool read(int *humidity,int *temperature);

		DhtState getLastResult()
		{
			return m_state;
		}
		
		const char *getLastResultMessage();
	private:
		unsigned char toByte(unsigned char *start);
		bool readingDone()
		{
			return m_bitIdx == DATABITS;
		}
		
		void clearReadState();
		void wakeSensor();
		void busyWait(uint32 micros);
		void isr();
		
		friend void __isrFunc(void *instance);
	};
	
	extern void __isrFunc(void *instance);
}
#endif