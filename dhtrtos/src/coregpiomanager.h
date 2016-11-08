/*
# Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com)
# Copyright (c) 2015/2016 Fabrizio Di Vittorio.
# All rights reserved.

# GNU GPL LICENSE
#
# This module is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; latest version thereof,
# available at: <http://www.gnu.org/licenses/gpl.txt>.
#
# This module is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this module; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA
*/

/*
Parts from gpio.h/gpio.c driver: Copyright (C) 2014 - 2016  Espressif System
*/


#ifndef _FDVGPIOMANAGER_H_
#define _FDVGPIOMANAGER_H_

extern "C"
{
	#include "esp8266/pin_mux_register.h"
	#include "esp8266/eagle_soc.h"
	#include "esp8266/gpio_register.h"
	#include "espressif/c_types.h"
	#include "esp8266/ets_sys.h"	
	#include "freertos/FreeRTOS.h"	
}

#define GPIO_PIN_ADDR(i)        (GPIO_PIN0_ADDRESS + i*4)

namespace core
{	

	typedef enum {
		GPIO_PIN_INTR_DISABLE = 0,      /**< disable GPIO interrupt */
		GPIO_PIN_INTR_POSEDGE = 1,      /**< GPIO interrupt type : rising edge */
		GPIO_PIN_INTR_NEGEDGE = 2,      /**< GPIO interrupt type : falling edge */
		GPIO_PIN_INTR_ANYEDGE = 3,      /**< GPIO interrupt type : bothe rising and falling edge */
		GPIO_PIN_INTR_LOLEVEL = 4,      /**< GPIO interrupt type : low level */
		GPIO_PIN_INTR_HILEVEL = 5       /**< GPIO interrupt type : high level */
	} GPIO_INT_TYPE;
	
	typedef void (*GPIOIsr)();
	
	
	class GPIOInterruptManager
	{
		
    private:
		GPIOIsr m_handlers[16];
		GPIO_INT_TYPE m_state[16];
        static uint32_t m_pinReg[16];
		
		GPIOInterruptManager();
		void setInterruptStatus(int gpionum,GPIO_INT_TYPE mode);
		bool isEnabledInterrupt();
		
		static void isr(void *);
		static void intr_state_set(uint32 i, GPIO_INT_TYPE intr_state);
		
	public:		
		void attachInterruptHandler(int gpionum,GPIOIsr handler,GPIO_INT_TYPE t);
		void detachInterupptHandler(int gpionum);
	
		static GPIOInterruptManager Manager; 
	};
}
#endif

