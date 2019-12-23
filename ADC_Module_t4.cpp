#include <Arduino.h>
#if defined(__IMXRT1062__)  // Teensy 4.0

#include "ADC.h"
#include "ADC_Module.h"

    
// Quick and dirty version to handle cases wehre callers made use of these modules...
void ADC_Module::setReference(ADC_REFERENCE type)
{
}



void ADC_Module::setResolution(uint8_t bits)
{
  uint32_t tmp32, mode;

  if (bits == 8) {
    // 8 bit conversion (17 clocks) plus 8 clocks for input settling
    mode = ADC_CFG_MODE(0) | ADC_CFG_ADSTS(3);
  } else if (bits == 0x88)  {
    // 8 bit fast conversion
    mode = ADC_CFG_MODE(0) | ADC_CFG_ADSTS(0);
  } else if (bits == 10) {
    // 10 bit conversion (17 clocks) plus 20 clocks for input settling
    mode = ADC_CFG_MODE(1) | ADC_CFG_ADSTS(2) | ADC_CFG_ADLSMP;
  } else {
    // 12 bit conversion (25 clocks) plus 24 clocks for input settling
    mode = ADC_CFG_MODE(2) | ADC_CFG_ADSTS(3) | ADC_CFG_ADLSMP;
  }

  __disable_irq();
  tmp32  = (_padc.CFG & (0xFFFFFC00));
  tmp32 |= (_padc.CFG & (0x03));  // ADICLK
  tmp32 |= (_padc.CFG & (0xE0));  // ADIV & ADLPC

  tmp32 |= mode;
  _padc.CFG = tmp32;
  __enable_irq();
}


uint8_t ADC_Module::getResolution()
{
  // bits 2-3:  00->8 01->10  10->12 
  return  8 + ((_padc.CFG >> 1) & 0x6); 
}


uint32_t ADC_Module::getMaxValue()
{
  switch (getResolution()) {
    case 8: return 255;
    case 10: return 1023;
    default: return 4095;
  }
}

void ADC_Module::setConversionSpeed(ADC_CONVERSION_SPEED speed)
{
  uint32_t adc_cfg_adsts;
  switch (speed) {
   case ADC_CONVERSION_SPEED::VERY_LOW_SPEED:    /*!< is guaranteed to be the lowest possible speed within specs for resolutions less than 16 bits (higher than 1 MHz). */
      adc_cfg_adsts = ADC_CFG_ADSTS(3);
      break;
   case ADC_CONVERSION_SPEED::LOW_SPEED:     /*!< is guaranteed to be the lowest possible speed within specs for all resolutions (higher than 2 MHz). */
      adc_cfg_adsts = ADC_CFG_ADSTS(2);
      break;
   case ADC_CONVERSION_SPEED::MED_SPEED:     /*!< is always >= LOW_SPEED and <= HIGH_SPEED. */
      adc_cfg_adsts = ADC_CFG_ADSTS(1);
      break;
   default:   
   case ADC_CONVERSION_SPEED::HIGH_SPEED:    /*!< is guaranteed to be the highest possible speed within specs for resolutions less than 16 bits (lower */
      adc_cfg_adsts = ADC_CFG_ADSTS(0);
    break;
      break;

  }
  __disable_irq();
  _padc.CFG = (_padc.CFG & ~(ADC_CFG_ADSTS(3))) | adc_cfg_adsts;    
  __enable_irq();
}


void ADC_Module::setSamplingSpeed(ADC_SAMPLING_SPEED speed)
{
  uint32_t adc_cfg_adlsmp = 0;
  switch (speed) {
    case ADC_SAMPLING_SPEED::VERY_LOW_SPEED: /*!< adds +16 ADCK. */
    case ADC_SAMPLING_SPEED::LOW_SPEED: /*!< adds +16 ADCK. */
      adc_cfg_adlsmp = ADC_CFG_ADLSMP;
      break;
    case ADC_SAMPLING_SPEED::MED_SPEED: /*!< adds +10 ADCK. */
      adc_cfg_adlsmp = ADC_CFG_ADLSMP | ADC_CFG_ADHSC;
      break;
    case ADC_SAMPLING_SPEED::VERY_HIGH_SPEED: /*!< adds +6 ADCK. */
    case ADC_SAMPLING_SPEED::HIGH_SPEED: /*!< adds +6 ADCK. */
      adc_cfg_adlsmp = ADC_CFG_ADHSC;
      break;
  }
  __disable_irq();
  _padc.CFG = (_padc.CFG & ~(ADC_CFG_ADLSMP | ADC_CFG_ADHSC)) | adc_cfg_adlsmp;    
  __enable_irq();
}


void ADC_Module::setAveraging(uint8_t num)
{
  uint32_t mode;

    __disable_irq();
    _padc.GC &= ~0x20;
    mode = _padc.CFG & ~0xC000;
    if (num >= 32) {
      mode |= ADC_CFG_AVGS(3);
    } else if (num >= 16) {
      mode |= ADC_CFG_AVGS(2);
    } else if (num >= 8) {
      mode |= ADC_CFG_AVGS(1);
    } else if (num >= 4) {
      mode |= ADC_CFG_AVGS(0);
    } else {
      mode |= 0;
    }
    _padc.CFG = mode;
    if (num >= 4) {
      _padc.GC |= ADC_GC_AVGE;// turns on averaging
    }
    __enable_irq();
}


void ADC_Module::attachInterrupt(void (*adc_isr)(void)) 
{
  // Probably should add this as part of constructor for module object... 
  IRQ_NUMBER_t irq = (&_padc.HC0 == &IMXRT_ADC1S.HC0)? IRQ_ADC1 : IRQ_ADC2;
  attachInterruptVector(irq, adc_isr);
  NVIC_ENABLE_IRQ(irq);
}

void ADC_Module::enableInterrupts()
{
    __disable_irq();
    _padc.HC0 |= ADC_HC_AIEN;  // enable the interrupt      
    __enable_irq();
}


void ADC_Module::disableInterrupts()
{
    __disable_irq();
    _padc.HC0 &= ~ADC_HC_AIEN;  // Disage the interrupt
    __enable_irq();
}


void ADC_Module::enableDMA()
{
    __disable_irq();
    _padc.GC |= (ADC_GC_DMAEN | ADC_GC_ADCO);  // enable DMA      
    __enable_irq();
}


void ADC_Module::disableDMA()
{
    __disable_irq();
    _padc.GC &= ~(ADC_GC_DMAEN | ADC_GC_ADCO);  // Disable DMA      
    __enable_irq();
}


bool ADC_Module::isConverting()
{
  return (_padc.GS & ADC_GS_ADACT) != 0;
}


bool ADC_Module::isComplete()
{
  return (_padc.HS & ADC_HS_COCO0) != 0;
}

elapsedMillis timeout;
int ADC_Module::analogRead(uint8_t pin, uint32_t TIMEOUT)
{
  if(!checkPin(pin)) {
    fail_flag |= ADC_ERROR::WRONG_PIN;
    return ADC_ERROR_VALUE;
  }

  timeout = 0;
  _padc.HC0 = (_padc.HC0 & ADC_HC_AIEN) | channel2sc1a[pin];
  //while (!(_padc.HS & ADC_HS_COCO0)) ; // wait
  if(TIMEOUT >= 0){
	  while (!(_padc.HS & ADC_HS_COCO0)){
			if(timeout > TIMEOUT){
				timeout = 0; // wait
				fail_flag |= ADC_ERROR::TIMEOUT;
				return ADC_ERROR_VALUE;
			}
	  }
	  fail_flag = ADC_ERROR::CLEAR;
	  return _padc.R0;
  } else {
	  while (!(_padc.HS & ADC_HS_COCO0));
	  return _padc.R0;
  }
  
}  


bool ADC_Module::startSingleRead(uint8_t pin)
{
  if(!checkPin(pin)) {
    fail_flag |= ADC_ERROR::WRONG_PIN;
    return false;
  }
  _padc.HC0 = (_padc.HC0 & ADC_HC_AIEN) | channel2sc1a[pin];
  return true;
}

bool ADC_Module::startContinuous(uint8_t pin) {

    if(!checkPin(pin)) {
      fail_flag |= ADC_ERROR::WRONG_PIN;
      return false;
    }

    // set continuous conversion flag
    __disable_irq();
    _padc.GC |=  ADC_GC_ADCO;  // enable continuous updates.      
    __enable_irq();

    _padc.HC0 = (_padc.HC0 & ADC_HC_AIEN) | channel2sc1a[pin];

    return true;
}


/* Starts continuous and differential conversion between the pins (pinP-pinN)
 * It returns as soon as the ADC is set, use analogReadContinuous() to read the value
 * Set the resolution, number of averages and voltage reference using the appropriate functions BEFORE calling this function
*/
bool ADC_Module::startContinuousDifferential(uint8_t pinP, uint8_t pinN) {

      fail_flag |= ADC_ERROR::OTHER;
      return false;   // all others are invalid
}


/* Stops continuous conversion
*/
void ADC_Module::stopContinuous() {

    __disable_irq();
    _padc.GC &=  ~ADC_GC_ADCO;  // enable continuous updates.      
    __enable_irq();

    return;
}

bool ADC_Module::checkPin(uint8_t pin) {

    if(pin>ADC_MAX_PIN) {
        return false;   // all others are invalid
    }

    // translate pin number to SC1A number, that also contains MUX a or b info.
    const uint8_t sc1a_pin = channel2sc1a[pin];

    // check for valid pin
    if(sc1a_pin == ADC_SC1A_PIN_INVALID ) {
        return false;   // all others are invalid
    }

    return true;
}


#endif
