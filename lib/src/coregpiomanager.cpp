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


#include "coregpio.h"
extern "C"
{
	#include "esp8266/pin_mux_register.h"
	#include "esp8266/eagle_soc.h"
	
void _isr()
{
	printf("GPIO0 - Int\n");	
}
	
}


namespace core
{
	uint32_t GPIOInterruptManager::m_pinReg[16] = {PERIPHS_IO_MUX_GPIO0_U, PERIPHS_IO_MUX_U0TXD_U, PERIPHS_IO_MUX_GPIO2_U, PERIPHS_IO_MUX_U0RXD_U, 
								  PERIPHS_IO_MUX_GPIO4_U, PERIPHS_IO_MUX_GPIO5_U, PERIPHS_IO_MUX_SD_CLK_U, PERIPHS_IO_MUX_SD_DATA0_U, 
								  PERIPHS_IO_MUX_SD_DATA1_U, PERIPHS_IO_MUX_SD_DATA2_U, PERIPHS_IO_MUX_SD_DATA3_U, PERIPHS_IO_MUX_SD_CMD_U, 
								  PERIPHS_IO_MUX_MTDI_U, PERIPHS_IO_MUX_MTCK_U, PERIPHS_IO_MUX_MTMS_U, PERIPHS_IO_MUX_MTDO_U};


	GPIOInterruptManager GPIOInterruptManager::Manager;
	GPIOInterruptManager::GPIOInterruptManager()
	{
		for(int i = 0;i < 16;i++)
		{
			m_handlers[i] = NULL;
			m_state[i] = GPIO_PIN_INTR_DISABLE;
		}
		
		_xt_isr_attach(ETS_GPIO_INUM, (_xt_isr)isr,NULL);
	}
	bool GPIOInterruptManager::isEnabledInterrupt()
	{
		for(int i = 0;i < 16;i++)
		{
			if(m_handlers[i] != NULL && m_state[i] != GPIO_PIN_INTR_DISABLE)
				return true;
		}
		return false;
	}
	
	void GPIOInterruptManager::isr(void *apa)
	{
		uint32_t gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);

	   for (int i = 0; i < 16; i++) 
	   {
		  if ((gpio_status & BIT(i)) ) 
		  {
			 //disable interrupt
			 intr_state_set(i, GPIO_PIN_INTR_DISABLE);
			 // call func here
		     if(GPIOInterruptManager::Manager.m_handlers[i] != NULL)
			 {
				 GPIOInterruptManager::Manager.m_handlers[i]();
			 }
							
			 //clear interrupt status
			 GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status & BIT(i));
			 
			 // restore
			 intr_state_set(i, GPIOInterruptManager::Manager.m_state[i]);
		  }
	   }   
	}
	void GPIOInterruptManager::intr_state_set(uint32 i, GPIO_INT_TYPE intr_state)
	{
		uint32 pin_reg;

		portENTER_CRITICAL();

		pin_reg = GPIO_REG_READ(GPIO_PIN_ADDR(i));
		pin_reg &= (~GPIO_PIN_INT_TYPE_MASK);
		pin_reg |= (intr_state << GPIO_PIN_INT_TYPE_LSB);
		GPIO_REG_WRITE(GPIO_PIN_ADDR(i), pin_reg);

		portEXIT_CRITICAL();
	}
	void GPIOInterruptManager::setInterruptStatus(int gpionum,GPIO_INT_TYPE mode)
	{
	   portENTER_CRITICAL();
	   
	   //clear interrupt status
	   GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(gpionum));

	   // set the mode   
	   intr_state_set(gpionum, mode);
	   
		if(isEnabledInterrupt())
		{
			_xt_isr_unmask(1<<ETS_GPIO_INUM);
		}
		else
		{
			_xt_isr_mask(1<<ETS_GPIO_INUM);
		}

	   portEXIT_CRITICAL();
	}
	void GPIOInterruptManager::attachInterruptHandler(int gpionum,GPIOIsr handler,GPIO_INT_TYPE t)
	{
		printf("Manager::atach(%d)",gpionum);
		m_handlers[gpionum] = handler;
		m_state[gpionum] = t;
		setInterruptStatus(gpionum,t);
	}
	
	void GPIOInterruptManager::detachInterupptHandler(int gpionum)
	{
		m_handlers[gpionum] = NULL;
		m_state[gpionum] = GPIO_PIN_INTR_DISABLE;
		setInterruptStatus(gpionum,GPIO_PIN_INTR_DISABLE);		
	}
	
}

