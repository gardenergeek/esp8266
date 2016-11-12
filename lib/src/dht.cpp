
extern "C"
{
#include "esp_common.h"
}

#include "coregpio.h"
#include "dht.h"



#define DATABITS 40
Dht22::Dht22(int pinNumber):m_gpio(pinNumber)
{
	clearReadState();
	m_lastReadTs = 0;
}
bool Dht22::configure()
{
	// Configure io-pin, we use pullup to keep high, altough an input
	m_gpio.modeInput();
	m_gpio.enablePullUp(true);	// Pull pin high
	
	// Attach irq handler
	m_gpio.attachInterruptHandler(Dht22::sharedIsr,this,core::GPIO_PIN_INTR_ANYEDGE);
	
	return true;
}
	
bool Dht22::read(int *humidity,int *temperature)
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
	
const char *Dht22::getLastResultMessage()
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

unsigned char Dht22::toByte(unsigned char *start)
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
	
void Dht22::clearReadState()
{
	m_lastValue = false;
	m_positiveFlankStart = 0;
	m_bitIdx = 0;
	m_state = DHTOK;
	m_gotResponseHigh = false;
}
void Dht22::wakeSensor()
{
	// Drive bus low
	m_gpio.modeOutput();	
	
	m_gpio.write(false);	
	busyWait(20000);
	
	m_gpio.write(true);	// Issue "go" to sensor		
	busyWait(40);
}
void Dht22::busyWait(uint32 micros)
{
	uint32 startTs = WDEV_NOW();
	
	while( (WDEV_NOW() - startTs) < micros);
}
void Dht22::isr()
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
