/*
    This file is part of Repetier-Firmware.

    Repetier-Firmware is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Repetier-Firmware is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Repetier-Firmware.  If not, see <http://www.gnu.org/licenses/>.

    This firmware is a nearly complete rewrite of the sprinter firmware
    by kliment (https://github.com/kliment/Sprinter)
    which based on Tonokip RepRap firmware rewrite based off of Hydra-mmm firmware.

  
    
    Main author: repetier
 
    Initial port of hardware abstraction layer to Arduino Due: John Silvia
*/

#include "Repetier.h"
#include <malloc.h>

//extern "C" void __cxa_pure_virtual() { }
extern "C" char *sbrk(int i);
extern long bresenham_step();

volatile uint8_t HAL::insideTimer1=0;
#ifndef DUE_SOFTWARE_SPI
    int spiDueDividors[] = {10,21,42,84,168,255,255};
#endif

HAL::HAL()
{
    //ctor
}

HAL::~HAL()
{
    //dtor
}


long HAL::CPUDivU2(unsigned int divisor)
{
    return F_CPU/divisor;
}

// Set up all timer interrupts 
void HAL::setupTimer() {
    uint32_t     tc_count, tc_clock;

    pmc_set_writeprotect(false);

    // set 3 bits for interrupt group priority, 2 bits for sub-priority
    NVIC_SetPriorityGrouping(4);

#if defined(USE_ADVANCE)
    // Timer for extruder control
    pmc_enable_periph_clk(EXTRUDER_TIMER_IRQ);  // enable power to timer
    NVIC_SetPriority((IRQn_Type)EXTRUDER_TIMER_IRQ, NVIC_EncodePriority(4, 1, 1));

    // count up to value in RC register using given clock
    TC_Configure(EXTRUDER_TIMER, EXTRUDER_TIMER_CHANNEL, TC_CMR_WAVSEL_UP_RC | TC_CMR_WAVE | TC_CMR_TCCLKS_TIMER_CLOCK4);

    TC_SetRC(EXTRUDER_TIMER, EXTRUDER_TIMER_CHANNEL, (F_CPU_TRUE / TIMER0_PRESCALE) / EXTRUDER_CLOCK_FREQ); // set frequency
    TC_Start(EXTRUDER_TIMER, EXTRUDER_TIMER_CHANNEL);           // start timer running
    
    // enable RC compare interrupt
    EXTRUDER_TIMER->TC_CHANNEL[EXTRUDER_TIMER_CHANNEL].TC_IER = TC_IER_CPCS;
    // clear the "disable RC compare" interrupt
    EXTRUDER_TIMER->TC_CHANNEL[EXTRUDER_TIMER_CHANNEL].TC_IDR = ~TC_IER_CPCS;

    // allow interrupts on timer
    NVIC_EnableIRQ((IRQn_Type)EXTRUDER_TIMER_IRQ);
#endif
    // Regular interrupts for heater control etc
    pmc_enable_periph_clk(PWM_TIMER_IRQ);
    NVIC_SetPriority((IRQn_Type)PWM_TIMER_IRQ, NVIC_EncodePriority(4, 3, 0));
   
    TC_FindMckDivisor(PWM_CLOCK_FREQ, F_CPU_TRUE, &tc_count, &tc_clock, F_CPU_TRUE);  
    TC_Configure(PWM_TIMER, PWM_TIMER_CHANNEL, TC_CMR_WAVSEL_UP_RC | TC_CMR_WAVE | tc_clock);

    TC_SetRC(PWM_TIMER, PWM_TIMER_CHANNEL, (F_CPU_TRUE / tc_count) / PWM_CLOCK_FREQ);
    TC_Start(PWM_TIMER, PWM_TIMER_CHANNEL);
 
    PWM_TIMER->TC_CHANNEL[PWM_TIMER_CHANNEL].TC_IER = TC_IER_CPCS;
    PWM_TIMER->TC_CHANNEL[PWM_TIMER_CHANNEL].TC_IDR = ~TC_IER_CPCS;
    NVIC_EnableIRQ((IRQn_Type)PWM_TIMER_IRQ);

    // Timer for stepper motor control
    pmc_enable_periph_clk(TIMER1_TIMER_IRQ );
    NVIC_SetPriority((IRQn_Type)TIMER1_TIMER_IRQ, NVIC_EncodePriority(4, 1, 0));
      
    TC_Configure(TIMER1_TIMER, TIMER1_TIMER_CHANNEL, TC_CMR_WAVSEL_UP_RC | 
                 TC_CMR_WAVE | TC_CMR_TCCLKS_TIMER_CLOCK1);

    TC_SetRC(TIMER1_TIMER, TIMER1_TIMER_CHANNEL, (F_CPU_TRUE / TIMER1_PRESCALE) / TIMER1_CLOCK_FREQ);
    TC_Start(TIMER1_TIMER, TIMER1_TIMER_CHANNEL);

    TIMER1_TIMER->TC_CHANNEL[TIMER1_TIMER_CHANNEL].TC_IER = TC_IER_CPCS;
    TIMER1_TIMER->TC_CHANNEL[TIMER1_TIMER_CHANNEL].TC_IDR = ~TC_IER_CPCS;
    NVIC_EnableIRQ((IRQn_Type)TIMER1_TIMER_IRQ); 

    // Servo control
#if FEATURE_SERVO
#if SERVO0_PIN>-1
    SET_OUTPUT(SERVO0_PIN);
    WRITE(SERVO0_PIN,LOW);
#endif
#if SERVO1_PIN>-1
    SET_OUTPUT(SERVO1_PIN);
    WRITE(SERVO1_PIN,LOW);
#endif
#if SERVO2_PIN>-1
    SET_OUTPUT(SERVO2_PIN);
    WRITE(SERVO2_PIN,LOW);
#endif
#if SERVO3_PIN>-1
    SET_OUTPUT(SERVO3_PIN);
    WRITE(SERVO3_PIN,LOW);
#endif
    pmc_enable_periph_clk(SERVO_TIMER_IRQ );
    NVIC_SetPriority((IRQn_Type)SERVO_TIMER_IRQ, NVIC_EncodePriority(4, 2, 0));
      
    TC_Configure(SERVO_TIMER, SERVO_TIMER_CHANNEL, TC_CMR_WAVSEL_UP_RC | 
                 TC_CMR_WAVE | TC_CMR_TCCLKS_TIMER_CLOCK1);

    TC_SetRC(SERVO_TIMER, SERVO_TIMER_CHANNEL, (F_CPU_TRUE / SERVO_PRESCALE) / SERVO_CLOCK_FREQ);
    TC_Start(SERVO_TIMER, SERVO_TIMER_CHANNEL);

    SERVO_TIMER->TC_CHANNEL[SERVO_TIMER_CHANNEL].TC_IER = TC_IER_CPCS;
    SERVO_TIMER->TC_CHANNEL[SERVO_TIMER_CHANNEL].TC_IDR = ~TC_IER_CPCS;
    NVIC_EnableIRQ((IRQn_Type)SERVO_TIMER_IRQ);
#endif
}



#if ANALOG_INPUTS>0
// Initialize ADC channels
void HAL::analogStart(void)
{
  uint32_t  adcEnable = 0;

  // ensure we can write to ADC registers
  ADC->ADC_WPMR = ADC_WPMR_WPKEY(0);
  pmc_enable_periph_clk(ID_ADC);  // enable adc clock

  for(int i=0; i<ANALOG_INPUTS; i++)
  {
      osAnalogInputCounter[i] = 0;
      osAnalogInputValues[i] = 0;

  // osAnalogInputChannels
      //adcEnable |= (0x1u << adcChannel[i]);
      adcEnable |= (0x1u << osAnalogInputChannels[i]);
  }

  // enable channels
  ADC->ADC_CHER = adcEnable;
  ADC->ADC_CHDR = !adcEnable;

  // Initialize ADC mode register (some of the following params are not used here)
  // HW trigger disabled, use external Trigger, 12 bit resolution
  // core and ref voltage stays on, normal sleep mode, normal not free-run mode
  // startup time 16 clocks, settling time 17 clocks, no changes on channel switch
  // convert channels in numeric order
  // set prescaler rate  MCK/((PRESCALE+1) * 2)
  // set tracking time  (TRACKTIM+1) * clock periods
  // set transfer period  (TRANSFER * 2 + 3) 
  ADC->ADC_MR = ADC_MR_TRGEN_DIS | ADC_MR_TRGSEL_ADC_TRIG0 | ADC_MR_LOWRES_BITS_10 |
            ADC_MR_SLEEP_NORMAL | ADC_MR_FWUP_OFF | ADC_MR_FREERUN_OFF |
            ADC_MR_STARTUP_SUT64 | ADC_MR_SETTLING_AST17 | ADC_MR_ANACH_NONE |
            ADC_MR_USEQ_NUM_ORDER |
            ADC_MR_PRESCAL(AD_PRESCALE_FACTOR) |
            ADC_MR_TRACKTIM(AD_TRACKING_CYCLES) |
            ADC_MR_TRANSFER(AD_TRANSFER_CYCLES);

  ADC->ADC_IER = 0;             // no ADC interrupts
  ADC->ADC_CGR = 0;             // Gain = 1
  ADC->ADC_COR = 0;             // Single-ended, no offset
  
  // start first conversion
  ADC->ADC_CR = ADC_CR_START;
}

#endif

// Print apparent cause of start/restart
void HAL::showStartReason() {
    int mcu = (RSTC->RSTC_SR & RSTC_SR_RSTTYP_Msk) >> RSTC_SR_RSTTYP_Pos;
    switch (mcu){
    case 0:
        Com::printInfoFLN(Com::tPowerUp);
        break;
    case 1:
        // this is return from backup mode on SAM
        Com::printInfoFLN(Com::tBrownOut);
    case 2:
        Com::printInfoFLN(Com::tWatchdog);
        break;
    case 3:
        Com::printInfoFLN(Com::tSoftwareReset);
        break;
    case 4:
        Com::printInfoFLN(Com::tExternalReset);
    } 
}

// Return available memory
int HAL::getFreeRam() {
    struct mallinfo memstruct = mallinfo();
    register char * stack_ptr asm ("sp");

    // avail mem in heap + (bottom of stack addr - end of heap addr)
    return (memstruct.fordblks + (int)stack_ptr -  (int)sbrk(0));
}

// Reset peripherals and cpu
void HAL::resetHardware() {
    RSTC->RSTC_CR = RSTC_CR_KEY(0xA5) | RSTC_CR_PERRST | RSTC_CR_PROCRST;
}


#ifndef DUE_SOFTWARE_SPI
   // hardware SPI
   void HAL::spiBegin()
   {
        // Configre SPI pins
        PIO_Configure(
           g_APinDescription[SCK_PIN].pPort,
           g_APinDescription[SCK_PIN].ulPinType,
           g_APinDescription[SCK_PIN].ulPin,
           g_APinDescription[SCK_PIN].ulPinConfiguration);
        PIO_Configure(
           g_APinDescription[MOSI_PIN].pPort,
           g_APinDescription[MOSI_PIN].ulPinType,
           g_APinDescription[MOSI_PIN].ulPin,
           g_APinDescription[MOSI_PIN].ulPinConfiguration);
        PIO_Configure(
           g_APinDescription[MISO_PIN].pPort,
           g_APinDescription[MISO_PIN].ulPinType,
           g_APinDescription[MISO_PIN].ulPin,
           g_APinDescription[MISO_PIN].ulPinConfiguration);

        // set master mode, peripheral select, fault detection
        SPI_Configure(SPI0, ID_SPI0, SPI_MR_MSTR | 
                     SPI_MR_MODFDIS | SPI_MR_PS);
       SPI_Enable(SPI0);
        PIO_Configure(
           g_APinDescription[SPI_PIN].pPort,
           g_APinDescription[SPI_PIN].ulPinType,
           g_APinDescription[SPI_PIN].ulPin,
           g_APinDescription[SPI_PIN].ulPinConfiguration);
        spiInit(1);
   }
   // spiClock is 0 to 6, relecting AVR clock dividers 2,4,8,16,32,64,128
   // Due can only go as slow as AVR divider 32 -- slowest Due clock is 329,412 Hz
    void HAL::spiInit(uint8_t spiClock) 
   {
        if(spiClock>4) spiClock = 1;
        // Set SPI mode 0, clock, select not active after transfer, with delay between transfers
        SPI_ConfigureNPCS(SPI0, SPI_CHAN, SPI_CSR_NCPHA |
                         SPI_CSR_CSAAT | SPI_CSR_SCBR(spiDueDividors[spiClock]) | 
                         SPI_CSR_DLYBCT(1));
       SPI_Enable(SPI0);
   }
    // Write single byte to SPI
   void HAL::spiSend(byte b) {
        // wait for transmit register empty
        while ((SPI0->SPI_SR & SPI_SR_TDRE) == 0);
        // write byte with address and end transmission flag
        SPI0->SPI_TDR = (uint32_t)b | SPI_PCS(SPI_CHAN) | SPI_TDR_LASTXFER;
        // wait for receive register 
        while ((SPI0->SPI_SR & SPI_SR_RDRF) == 0);
        // clear status
        SPI0->SPI_RDR;
    }
   void HAL::spiSend(const uint8_t* buf , size_t n)
   {
       if (n == 0) return;
       for (int i=0; i<n-1; i++)
       {
           while ((SPI0->SPI_SR & SPI_SR_TDRE) == 0);
           SPI0->SPI_TDR = (uint32_t)buf[i] | SPI_PCS(SPI_CHAN);
           while ((SPI0->SPI_SR & SPI_SR_RDRF) == 0);
           SPI0->SPI_RDR;
       }
       spiSend(buf[n-1]);
   }

    // Read single byte from SPI
   uint8_t HAL::spiReceive()
   {
        // wait for transmit register empty
        while ((SPI0->SPI_SR & SPI_SR_TDRE) == 0);
        // write dummy byte with address and end transmission flag
        SPI0->SPI_TDR = 0x000000FF | SPI_PCS(SPI_CHAN) | SPI_TDR_LASTXFER;

        // wait for receive register 
        while ((SPI0->SPI_SR & SPI_SR_RDRF) == 0);
        // get byte from receive register
        return SPI0->SPI_RDR;
   }
    // Read from SPI into buffer
   void HAL::spiReadBlock(uint8_t*buf,uint16_t nbyte) 
   {     
       if (nbyte-- == 0) return;

       for (int i=0; i<nbyte; i++)
        {
           while ((SPI0->SPI_SR & SPI_SR_TDRE) == 0);
           SPI0->SPI_TDR = 0x000000FF | SPI_PCS(SPI_CHAN);
           while ((SPI0->SPI_SR & SPI_SR_RDRF) == 0);
           buf[i] = SPI0->SPI_RDR;
        }
       buf[nbyte] = spiReceive();
   }

    // Write from buffer to SPI

   void HAL::spiSendBlock(uint8_t token, const uint8_t* buf)
   {
       while ((SPI0->SPI_SR & SPI_SR_TDRE) == 0);
       SPI0->SPI_TDR = (uint32_t)token | SPI_PCS(SPI_CHAN);
       while ((SPI0->SPI_SR & SPI_SR_RDRF) == 0);
       SPI0->SPI_RDR;
       for (int i=0; i<511; i++)
       {
           while ((SPI0->SPI_SR & SPI_SR_TDRE) == 0);
           SPI0->SPI_TDR = (uint32_t)buf[i] | SPI_PCS(SPI_CHAN);
           while ((SPI0->SPI_SR & SPI_SR_RDRF) == 0);
           SPI0->SPI_RDR;
       }
       spiSend(buf[511]);
   }
#endif

/*************************************************************************
 Initialization of the I2C bus interface. Need to be called only once
*************************************************************************/
void HAL::i2cInit(unsigned long clockSpeedHz)
{
    // enable TWI                                                        
    pmc_enable_periph_clk(TWI_ID);                                       
                                                                         
    // Configure pins                                                    
    PIO_Configure(g_APinDescription[SDA_PIN].pPort,                      
                  g_APinDescription[SDA_PIN].ulPinType,                  
                  g_APinDescription[SDA_PIN].ulPin,                      
                  g_APinDescription[SDA_PIN].ulPinConfiguration);        
    PIO_Configure(g_APinDescription[SCL_PIN].pPort,                      
                  g_APinDescription[SCL_PIN].ulPinType,                  
                  g_APinDescription[SCL_PIN].ulPin,                      
                  g_APinDescription[SCL_PIN].ulPinConfiguration);        
                                                                         
    // Set to Master mode with known state                               
    TWI_INTERFACE->TWI_CR = TWI_CR_SVEN;                                 
    TWI_INTERFACE->TWI_CR = TWI_CR_SWRST;                                
    TWI_INTERFACE->TWI_RHR;                                              
    TWI_INTERFACE->TWI_IMR = 0;                                          
                                                                         
    TWI_INTERFACE->TWI_CR = TWI_CR_SVDIS;                                
    TWI_INTERFACE->TWI_CR = TWI_CR_MSDIS;                                
    TWI_INTERFACE->TWI_CR = TWI_CR_MSEN;                                 
                                                                         
    // Set i2c clock rate                                                
    uint32_t dwCkDiv = 0;                                                
    uint32_t dwClDiv;                                                    
    while ( dwClDiv == 0 )                                               
    {                                                                    
        dwClDiv = ((F_CPU_TRUE / (2 * clockSpeedHz)) - 4) / (1<<dwCkDiv);
                                                                         
        if ( dwClDiv > 255 )                                             
        {                                                                
            dwCkDiv++;                                                   
            dwClDiv = 0;                                                 
        }                                                                
    }                                                                    
    TWI_INTERFACE->TWI_CWGR = 0;                                         
    TWI_INTERFACE->TWI_CWGR = (dwCkDiv << 16) | (dwClDiv << 8) | dwClDiv;
}


/*************************************************************************
  Issues a start condition and sends address and transfer direction.
  return 0 = device accessible, 1= failed to access device
*************************************************************************/
unsigned char HAL::i2cStart(unsigned char address_and_direction)
{
    uint32_t twiDirection = address_and_direction & 1;
    uint32_t address = address_and_direction >> 1;

    TWI_INTERFACE->TWI_CR = TWI_CR_MSEN | TWI_CR_SVDIS;

    // set master mode register with no internal address
    TWI_INTERFACE->TWI_MMR = 0;
    TWI_INTERFACE->TWI_MMR = (twiDirection << 12) | TWI_MMR_IADRSZ_NONE |  
        TWI_MMR_DADR(address);

    // returning readiness to send/recieve not device accessibility
    // return value not used in code anyway
    return !(TWI_INTERFACE->TWI_SR & TWI_SR_TXCOMP);
}


/*************************************************************************
 Issues a start condition and sends address and transfer direction.
 If device is busy, use ack polling to wait until device is ready

 Input:   address and transfer direction of I2C device
*************************************************************************/
void HAL::i2cStartWait(unsigned char address_and_direction)
{
     uint32_t twiDirection = address_and_direction & 1;
    uint32_t address = address_and_direction >> 1;

    while(!(TWI_INTERFACE->TWI_SR & TWI_SR_TXCOMP));

    // set to master mode
    TWI_INTERFACE->TWI_CR = TWI_CR_MSEN | TWI_CR_SVDIS;

    // set master mode register with no internal address
    TWI_INTERFACE->TWI_MMR = 0;
    TWI_INTERFACE->TWI_MMR = (twiDirection << 12) | TWI_MMR_IADRSZ_NONE |  
         TWI_MMR_DADR(address);
}

/*************************************************************************
 Issues a start condition and sends address and transfer direction.
 Also specifies internal address of device

 Input:   address and transfer direction of I2C device, internal address
*************************************************************************/
void HAL::i2cStartAddr(unsigned char address_and_direction, unsigned int pos)
{
    uint32_t twiDirection = address_and_direction & 1;
    uint32_t address = address_and_direction >> 1;
    
    // if 1 byte address, eeprom uses lower address bits for pos > 255    
    if (EEPROM_ADDRSZ_BYTES == TWI_MMR_IADRSZ_1_BYTE)
    {
      address |= pos >> 8;
      pos &= 0xFF;
    }

    // set master mode register with internal address
    TWI_INTERFACE->TWI_MMR = 0;
    TWI_INTERFACE->TWI_MMR = (twiDirection << 12) | EEPROM_ADDRSZ_BYTES |
         TWI_MMR_DADR(address);

    // write internal address register
    TWI_INTERFACE->TWI_IADR = 0;
    TWI_INTERFACE->TWI_IADR = TWI_IADR_IADR(pos);
}

/*************************************************************************
 Terminates the data transfer and releases the I2C bus
*************************************************************************/
void HAL::i2cStop(void)
{
    i2cTxFinished();
    TWI_INTERFACE->TWI_CR = TWI_CR_STOP; 
    i2cCompleted ();
}

/*************************************************************************
 Signal start of data transfer
*************************************************************************/
void HAL::i2cStartBit(void)
{
    TWI_INTERFACE->TWI_CR = TWI_CR_START; 
}

/*************************************************************************
 Wait for transaction to complete
*************************************************************************/
void HAL::i2cCompleted (void)
{
    while(!((TWI_INTERFACE->TWI_SR & TWI_SR_TXCOMP) == TWI_SR_TXCOMP));
}

/*************************************************************************
 Wait for transmission to complete
*************************************************************************/
void HAL::i2cTxFinished(void)
{
    while( (TWI_INTERFACE->TWI_SR & TWI_SR_TXRDY) != TWI_SR_TXRDY);
}


/*************************************************************************
  Send one byte to I2C device

  Input:    byte to be transfered
  Return:   0 write successful
            1 write failed
*************************************************************************/
unsigned char HAL::i2cWrite( uint8_t data )
{    
  i2cWriting(data);
  TWI_INTERFACE->TWI_CR = TWI_CR_STOP;
  i2cTxFinished();
  unsigned char rslt = (TWI_INTERFACE->TWI_SR & TWI_SR_NACK) == TWI_SR_NACK;
  i2cCompleted ();
  return rslt;
}

/*************************************************************************
  Send one byte to I2C device
  Transaction can continue with more writes or reads 
************************************************************************/
void HAL::i2cWriting( uint8_t data )
{    
    TWI_INTERFACE->TWI_THR = data;
}


/*************************************************************************
 Read one byte from the I2C device, request more data from device
 Return:  byte read from I2C device
*************************************************************************/
unsigned char HAL::i2cReadAck(void)
{
    while( (TWI_INTERFACE->TWI_SR & TWI_SR_RXRDY) != TWI_SR_RXRDY );
    return TWI_INTERFACE->TWI_RHR;
}

/*************************************************************************
 Read one byte from the I2C device, read is followed by a stop condition

 Return:  byte read from I2C device
*************************************************************************/
unsigned char HAL::i2cReadNak(void)
{
    TWI_INTERFACE->TWI_CR = TWI_CR_STOP;
    
    while( (TWI_INTERFACE->TWI_SR & TWI_SR_RXRDY) != TWI_SR_RXRDY );
    unsigned char data = i2cReadAck();
    i2cCompleted();
    return data;
}


// Wait for X microseconds 
// this could be simpler but its used inside interrupts so must be reentrant
void HAL::microsecondsWait(uint32_t us) 
{
    uint32_t usStart, goal;

    // get the current count
    usStart =  TC_ReadCV(DELAY_TIMER, DELAY_TIMER_CHANNEL);

    // funny math here to give good accuracy with no overflow 
    goal = usStart + ((F_CPU_TRUE / (DELAY_TIMER_PRESCALE * 100000)) * us) / 10;

    // goal may have wrapped, if so wait for counter to catch up
    if(goal < usStart) {
        while(goal < TC_ReadCV(DELAY_TIMER, DELAY_TIMER_CHANNEL));
    }   
    // wait for counter to reach requested value
    while(goal > TC_ReadCV(DELAY_TIMER, DELAY_TIMER_CHANNEL));
}



#if FEATURE_SERVO
// may need further restrictions here in the future
#if defined (__SAM3X8E__)
unsigned int HAL::servoTimings[4] = {0,0,0,0};
static uint8_t servoIndex = 0;
void HAL::servoMicroseconds(uint8_t servo,int microsec) {
    if(microsec<500) microsec = 0;
    if(microsec>2500) microsec = 2500;
    servoTimings[servo] = (unsigned int)(((F_CPU_TRUE / SERVO_PRESCALE) / 
                                         1000000) * microsec);
}
 

// ================== Interrupt handling ======================

// Servo timer Interrupt handler
void SERVO_COMPA_VECTOR ()
{
    static uint32_t     interval;

  // apparently have to read status register
  TC_GetStatus(SERVO_TIMER, SERVO_TIMER_CHANNEL);

  switch(servoIndex) {
  case 0:
      if(HAL::servoTimings[0]) {
#if SERVO0_PIN>-1
          WRITE(SERVO0_PIN,HIGH);
#endif  
          interval =  HAL::servoTimings[0];
      } else 
          interval = SERVO2500US;
      TC_SetRC(SERVO_TIMER, SERVO_TIMER_CHANNEL, interval);
      break;
  case 1:
#if SERVO0_PIN>-1
      WRITE(SERVO0_PIN,LOW);
#endif
      TC_SetRC(SERVO_TIMER, SERVO_TIMER_CHANNEL, 
               SERVO5000US - interval);
    break;
  case 2:
      if(HAL::servoTimings[1]) {
#if SERVO1_PIN>-1
        WRITE(SERVO1_PIN,HIGH);
#endif
        interval =  HAL::servoTimings[1];
      } else 
          interval = SERVO2500US;
    TC_SetRC(SERVO_TIMER, SERVO_TIMER_CHANNEL, interval);
    break;
  case 3:
#if SERVO1_PIN>-1
      WRITE(SERVO1_PIN,LOW);
#endif
      TC_SetRC(SERVO_TIMER, SERVO_TIMER_CHANNEL, 
               SERVO5000US - interval);
    break;
  case 4:
      if(HAL::servoTimings[2]) {
#if SERVO2_PIN>-1
        WRITE(SERVO2_PIN,HIGH);
#endif
        interval =  HAL::servoTimings[2];
      } else  
          interval = SERVO2500US;
    TC_SetRC(SERVO_TIMER, SERVO_TIMER_CHANNEL, interval);
    break;
  case 5:
#if SERVO2_PIN>-1
      WRITE(SERVO2_PIN,LOW);
#endif
      TC_SetRC(SERVO_TIMER, SERVO_TIMER_CHANNEL, 
               SERVO5000US - interval);
    break;
  case 6:
      if(HAL::servoTimings[3]) {
#if SERVO3_PIN>-1
        WRITE(SERVO3_PIN,HIGH);
#endif
        interval =  HAL::servoTimings[3];
      } else 
          interval = SERVO2500US;
    TC_SetRC(SERVO_TIMER, SERVO_TIMER_CHANNEL, interval);
    break;
  case 7:
#if SERVO3_PIN>-1
      WRITE(SERVO3_PIN,LOW);
#endif
      TC_SetRC(SERVO_TIMER, SERVO_TIMER_CHANNEL, 
               SERVO5000US - interval);
    break;
  }
  servoIndex++;
  if(servoIndex>7) servoIndex = 0;
}
#else
#error No servo support for your board, please diable FEATURE_SERVO
#endif
#endif

/** \brief Sets the timer 1 compare value to delay ticks.
*/
inline void setTimer(unsigned long delay)
{
    // convert old AVR timer delay value for SAM timers
    uint32_t timer_count = (delay * TIMER1_PRESCALE);

    if(timer_count == 0) timer_count = 1;
    TC_SetRC(TIMER1_TIMER, TIMER1_TIMER_CHANNEL, timer_count);
    TC_Start(TIMER1_TIMER, TIMER1_TIMER_CHANNEL);
}

/** \brief Timer interrupt routine to drive the stepper motors.
*/
void TIMER1_COMPA_VECTOR ()
{
    // apparently have to read status register
    TC_GetStatus(TIMER1_TIMER, TIMER1_TIMER_CHANNEL);
    if(HAL::insideTimer1) return;
    HAL::insideTimer1 = 1;
    if(PrintLine::hasLines())
    {
        setTimer(PrintLine::bresenhamStep());
        HAL::allowInterrupts();
    }
    else
    {
        if(waitRelax==0)
        {
#ifdef USE_ADVANCE
            if(Printer::advanceStepsSet)
            {
                Printer::extruderStepsNeeded-=Printer::advanceStepsSet;
#ifdef ENABLE_QUADRATIC_ADVANCE
                Printer::advanceExecuted = 0;
#endif
                Printer::advanceStepsSet = 0;
            }
            if((!Printer::extruderStepsNeeded) && (DISABLE_E)) 
                Extruder::disableCurrentExtruderMotor();
#else
            if(DISABLE_E) Extruder::disableCurrentExtruderMotor();
#endif
        }
        else waitRelax--;
    }
    DEBUG_MEMORY;
    HAL::insideTimer1=0;
}

/**
This timer is called 3906 times per second. It is used to update
pwm values for heater and some other frequent jobs. 
*/
void PWM_TIMER_VECTOR ()
{
    // apparently have to read status register
    TC_GetStatus(PWM_TIMER, PWM_TIMER_CHANNEL);

    static uint8_t pwm_count = 0;
    static uint8_t pwm_pos_set[NUM_EXTRUDER+3];
    static uint8_t pwm_cooler_pos_set[NUM_EXTRUDER];

    if(pwm_count==0)
    {
#if EXT0_HEATER_PIN>-1
        if((pwm_pos_set[0] = pwm_pos[0])>0) WRITE(EXT0_HEATER_PIN,1);
#if EXT0_EXTRUDER_COOLER_PIN>-1
        if((pwm_cooler_pos_set[0] = extruder[0].coolerPWM)>0) WRITE(EXT0_EXTRUDER_COOLER_PIN,1);
#endif
#endif
#if defined(EXT1_HEATER_PIN) && EXT1_HEATER_PIN>-1 && NUM_EXTRUDER>1
        if((pwm_pos_set[1] = pwm_pos[1])>0) WRITE(EXT1_HEATER_PIN,1);
#if EXT1_EXTRUDER_COOLER_PIN>-1
        if((pwm_cooler_pos_set[1] = extruder[1].coolerPWM)>0) WRITE(EXT1_EXTRUDER_COOLER_PIN,1);
#endif
#endif
#if defined(EXT2_HEATER_PIN) && EXT2_HEATER_PIN>-1 && NUM_EXTRUDER>2
        if((pwm_pos_set[2] = pwm_pos[2])>0) WRITE(EXT2_HEATER_PIN,1);
#if EXT2_EXTRUDER_COOLER_PIN>-1
        if((pwm_cooler_pos_set[2] = extruder[2].coolerPWM)>0) WRITE(EXT2_EXTRUDER_COOLER_PIN,1);
#endif
#endif
#if defined(EXT3_HEATER_PIN) && EXT3_HEATER_PIN>-1 && NUM_EXTRUDER>3
        if((pwm_pos_set[3] = pwm_pos[3])>0) WRITE(EXT3_HEATER_PIN,1);
#if EXT3_EXTRUDER_COOLER_PIN>-1
        if((pwm_cooler_pos_set[3] = extruder[3].coolerPWM)>0) WRITE(EXT3_EXTRUDER_COOLER_PIN,1);
#endif
#endif
#if defined(EXT4_HEATER_PIN) && EXT4_HEATER_PIN>-1 && NUM_EXTRUDER>4
        if((pwm_pos_set[4] = pwm_pos[4])>0) WRITE(EXT4_HEATER_PIN,1);
#if EXT4_EXTRUDER_COOLER_PIN>-1
        if((pwm_cooler_pos_set[4] = pwm_pos[4].coolerPWM)>0) WRITE(EXT4_EXTRUDER_COOLER_PIN,1);
#endif
#endif
#if defined(EXT5_HEATER_PIN) && EXT5_HEATER_PIN>-1 && NUM_EXTRUDER>5
        if((pwm_pos_set[5] = pwm_pos[5])>0) WRITE(EXT5_HEATER_PIN,1);
#if EXT5_EXTRUDER_COOLER_PIN>-1
        if((pwm_cooler_pos_set[5] = extruder[5].coolerPWM)>0) WRITE(EXT5_EXTRUDER_COOLER_PIN,1);
#endif
#endif
#if FAN_BOARD_PIN>-1
        if((pwm_pos_set[NUM_EXTRUDER+1] = pwm_pos[NUM_EXTRUDER+1])>0) WRITE(FAN_BOARD_PIN,1);
#endif
#if FAN_PIN>-1 && FEATURE_FAN_CONTROL
        if((pwm_pos_set[NUM_EXTRUDER+2] = pwm_pos[NUM_EXTRUDER+2])>0) WRITE(FAN_PIN,1);
#endif
#if HEATED_BED_HEATER_PIN>-1 && HAVE_HEATED_BED
        if((pwm_pos_set[NUM_EXTRUDER] = pwm_pos[NUM_EXTRUDER])>0) WRITE(HEATED_BED_HEATER_PIN,1);
#endif
    }
#if EXT0_HEATER_PIN>-1
    if(pwm_pos_set[0] == pwm_count && pwm_pos_set[0]!=255) WRITE(EXT0_HEATER_PIN,0);
#if EXT0_EXTRUDER_COOLER_PIN>-1
    if(pwm_cooler_pos_set[0] == pwm_count && pwm_cooler_pos_set[0]!=255) WRITE(EXT0_EXTRUDER_COOLER_PIN,0);
#endif
#endif
#if defined(EXT1_HEATER_PIN) && EXT1_HEATER_PIN>-1 && NUM_EXTRUDER>1
    if(pwm_pos_set[1] == pwm_count && pwm_pos_set[1]!=255) WRITE(EXT1_HEATER_PIN,0);
#if EXT1_EXTRUDER_COOLER_PIN>-1
    if(pwm_cooler_pos_set[1] == pwm_count && pwm_cooler_pos_set[1]!=255) WRITE(EXT1_EXTRUDER_COOLER_PIN,0);
#endif
#endif
#if defined(EXT2_HEATER_PIN) && EXT2_HEATER_PIN>-1 && NUM_EXTRUDER>2
    if(pwm_pos_set[2] == pwm_count && pwm_pos_set[2]!=255) WRITE(EXT2_HEATER_PIN,0);
#if EXT2_EXTRUDER_COOLER_PIN>-1
    if(pwm_cooler_pos_set[2] == pwm_count && pwm_cooler_pos_set[2]!=255) WRITE(EXT2_EXTRUDER_COOLER_PIN,0);
#endif
#endif
#if defined(EXT3_HEATER_PIN) && EXT3_HEATER_PIN>-1 && NUM_EXTRUDER>3
    if(pwm_pos_set[3] == pwm_count && pwm_pos_set[3]!=255) WRITE(EXT3_HEATER_PIN,0);
#if EXT3_EXTRUDER_COOLER_PIN>-1
    if(pwm_cooler_pos_set[3] == pwm_count && pwm_cooler_pos_set[3]!=255) WRITE(EXT3_EXTRUDER_COOLER_PIN,0);
#endif
#endif
#if defined(EXT4_HEATER_PIN) && EXT4_HEATER_PIN>-1 && NUM_EXTRUDER>4
    if(pwm_pos_set[4] == pwm_count && pwm_pos_set[4]!=255) WRITE(EXT4_HEATER_PIN,0);
#if EXT4_EXTRUDER_COOLER_PIN>-1
    if(pwm_cooler_pos_set[4] == pwm_count && pwm_cooler_pos_set[4]!=255) WRITE(EXT4_EXTRUDER_COOLER_PIN,0);
#endif
#endif
#if defined(EXT5_HEATER_PIN) && EXT5_HEATER_PIN>-1 && NUM_EXTRUDER>5
    if(pwm_pos_set[5] == pwm_count && pwm_pos_set[5]!=255) WRITE(EXT5_HEATER_PIN,0);
#if EXT5_EXTRUDER_COOLER_PIN>-1
    if(pwm_cooler_pos_set[5] == pwm_count && pwm_cooler_pos_set[5]!=255) WRITE(EXT5_EXTRUDER_COOLER_PIN,0);
#endif
#endif
#if FAN_BOARD_PIN>-1
    if(pwm_pos_set[NUM_EXTRUDER+2] == pwm_count && pwm_pos_set[NUM_EXTRUDER+2]!=255) WRITE(FAN_BOARD_PIN,0);
#endif
#if FAN_PIN>-1 && FEATURE_FAN_CONTROL
    if(pwm_pos_set[NUM_EXTRUDER+2] == pwm_count && pwm_pos_set[NUM_EXTRUDER+2]!=255) WRITE(FAN_PIN,0);
#endif
#if HEATED_BED_HEATER_PIN>-1 && HAVE_HEATED_BED
    if(pwm_pos_set[NUM_EXTRUDER] == pwm_count && pwm_pos_set[NUM_EXTRUDER]!=255) WRITE(HEATED_BED_HEATER_PIN,0);
#endif
    HAL::allowInterrupts();
    counterPeriodical++; // Appxoimate a 100ms timer
    if(counterPeriodical >= 390) //  (int)(F_CPU/40960))
    {
        counterPeriodical=0;
        executePeriodical=1;
    }
// read analog values -- only read one per interrupt
#if ANALOG_INPUTS>0
        
    // conversion finished?
    //if(ADC->ADC_ISR & ADC_ISR_EOC(adcChannel[osAnalogInputPos])) 
    if(ADC->ADC_ISR & ADC_ISR_EOC(osAnalogInputChannels[osAnalogInputPos])) 
    {                
      //osAnalogInputChannels
        //osAnalogInputBuildup[osAnalogInputPos] += ADC->ADC_CDR[adcChannel[osAnalogInputPos]]; 
        osAnalogInputBuildup[osAnalogInputPos] += ADC->ADC_CDR[osAnalogInputChannels[osAnalogInputPos]]; 
        if(++osAnalogInputCounter[osAnalogInputPos] >= (1 << ANALOG_INPUT_SAMPLE))
        {
#if ANALOG_INPUT_BITS+ANALOG_INPUT_SAMPLE<12
            osAnalogInputValues[osAnalogInputPos] =
                osAnalogInputBuildup[osAnalogInputPos] <<
                (12-ANALOG_INPUT_BITS-ANALOG_INPUT_SAMPLE);
#endif
#if ANALOG_INPUT_BITS+ANALOG_INPUT_SAMPLE>12
            osAnalogInputValues[osAnalogInputPos] =
                osAnalogInputBuildup[osAnalogInputPos] >>
                (ANALOG_INPUT_BITS+ANALOG_INPUT_SAMPLE-12);
#endif
#if ANALOG_INPUT_BITS+ANALOG_INPUT_SAMPLE==12
            osAnalogInputValues[osAnalogInputPos] =
                osAnalogInputBuildup[osAnalogInputPos];
#endif
            osAnalogInputBuildup[osAnalogInputPos] = 0;
            osAnalogInputCounter[osAnalogInputPos] = 0;
        }
        // Start next conversion cycle
        if(++osAnalogInputPos>=ANALOG_INPUTS) { 
            osAnalogInputPos = 0;
            ADC->ADC_CR = ADC_CR_START;
        }
    }
#endif
    UI_FAST; // Short timed user interface action
    pwm_count++;
}


/** \brief Timer routine for extruder stepper.

Several methods need to move the extruder. To get a optimal 
result, all methods update the printer_state.extruderStepsNeeded 
with the number of additional steps needed. During this 
interrupt, one step is executed. This will keep the extruder 
moving, until the total wanted movement is achieved. This will 
be done with the maximum allowable speed for the extruder. 
*/
#if defined(USE_ADVANCE)
// EXTRUDER_TIMER IRQ handler
void EXTRUDER_TIMER_VECTOR ()
{
    static int8_t extruderLastDirection = 0;
    // apparently have to read status register
    TC_GetStatus(EXTRUDER_TIMER, EXTRUDER_TIMER_CHANNEL);

    if(!Printer::isAdvanceActivated()) return; // currently no need
    // get current extruder timer count value
    uint32_t timer = EXTRUDER_TIMER->TC_CHANNEL[EXTRUDER_TIMER_CHANNEL].TC_RC;

    if(!Printer::isAdvanceActivated()) return; // currently no need
    if(Printer::extruderStepsNeeded > 0 && extruderLastDirection!=1)
    {
        Extruder::setDirection(true);
        extruderLastDirection = 1;
        timer += 40; // Add some more wait time to prevent blocking
    }

    else if(Printer::extruderStepsNeeded < 0 && extruderLastDirection!=-1)
    {
        Extruder::setDirection(false);
        extruderLastDirection = -1;
        timer += 40; // Add some more wait time to prevent blocking
    }
    else if(Printer::extruderStepsNeeded != 0)
    {
        Extruder::step();
        Printer::extruderStepsNeeded -= extruderLastDirection;
        Printer::insertStepperHighDelay();
        Extruder::unstep();
    }

    TC_SetRC(EXTRUDER_TIMER, EXTRUDER_TIMER_CHANNEL, timer);
}
#endif

// IRQ handler for tone generator
void BEEPER_TIMER_VECTOR () {
    static bool     toggle;

    TC_GetStatus(BEEPER_TIMER, BEEPER_TIMER_CHANNEL);

    WRITE(tone_pin, toggle);
    toggle = !toggle;
}













