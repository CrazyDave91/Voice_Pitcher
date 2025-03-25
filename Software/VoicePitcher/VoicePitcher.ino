#include <avr/io.h>
#include <avr/interrupt.h>
#define F_CPU 16000000L
#include <util/delay.h>

#define LED 13
#define MICRO A0
#define SPEAKER 3
#define POTI A1
#define POTI_PLUS A4
#define POTI_MINUS A5

#define FILT_400Hz ((uint16_t)(0.11 * 256.0))
#define FILT_4kHz ((uint16_t)(0.65 * 256.0))
#define FILT_8kHz ((uint16_t)(0.80 * 256.0))
#define FILT_DIV ((uint16_t)(256))

#define TIMER_BASE_DIV 13u // Base Frequency = 16Mhz / 64 Pre / DIV = 19,2kHz

#define PITCH_BUFFER_LEN 800

uint8_t sampleBuffer[PITCH_BUFFER_LEN] = {0};
uint16_t inputPointer = 0;
uint16_t outputPointer = 0;
volatile uint8_t outputSample = 128u;
bool continuousPitchMode = true;

/*###############################################
              Initialization
#################################################*/

void setup() {
  initPins();
  initMic();
  initSpeaker();
  initPitchMode();
}

void initPins(void)
{ 
  pinMode(LED, OUTPUT);
  pinMode(SPEAKER, OUTPUT);
  pinMode(MICRO, INPUT);
  pinMode(POTI, INPUT);
  pinMode(POTI_PLUS, OUTPUT);
  pinMode(POTI_MINUS, OUTPUT);
  digitalWrite(POTI_PLUS, HIGH);
  digitalWrite(POTI_MINUS, LOW);
  sei();
}

void initMic(void)
{
  // Init Timer 1 to synch input sampling
  TCCR1A = 0x00;                            // no output & CTC with OCR1A
  TCCR1B = (1<<WGM12)|(1<<CS11)|(1<<CS10);  // CTC with OCR1A & Prescaler 8
  OCR1AH = 0;                               // Frequency = 16Mhz / 64 Pre / OCRA
  OCR1AL = TIMER_BASE_DIV;
  TIMSK1 = (1<<OCIE1A);                     // enable OCRA interrupt
  
  // Init ADC to auto sample
  initAdc0FreeRun();
}

void initAdc0FreeRun(void)
{
  ADCSRA = 0;                                         // Stop ADC and terminate previous conversion
  ADMUX = (1<<REFS0)|(1<<ADLAR);                      // Vref=Vsup & Left Aligned & ADC0 selected
  ADCSRA = (1<<ADEN)|(1<<ADSC)|(1<<ADATE)|(1<<ADPS2); // ADC Enable & Autotrigger & ADC div 16
  ADCSRB = 0;                                         // Free running mode
  DIDR0 = (1<<ADC0D);                                 // Disable digital input buffer of ADC0
}

void initSpeaker(void)
{ 
  // Init Timer 0 to synch output sampling
  TCCR0A = (1<<WGM01);          // no output & CTC with OCR0A 
  TCCR0B = (1<<CS01)|(1<<CS00); // CTC with OCR0A & Prescaler 64
  OCR0A = TIMER_BASE_DIV;       // Frequency = 16Mhz / 64 Pre / OCRA
  TIMSK0 = (1<<OCIE0A);         // enable OCRA interrupt

  // Init Timer 2 for 1bit DAC on pin 3 aka. PD3 connected to OC2B
  TCCR2A = (1<<COM2B1)|(1<<WGM21)|(1<<WGM20); // Fast PWM to Output OC2B set at bottom
  TCCR2B = (1<<CS20);                         // Clock Divider 1
  TIMSK2 = (1<<TOIE2);                        // Timer 2 OVF enable 
}

void initPitchMode(void)
{
  uint8_t poti = readAdcChannel1();
  continuousPitchMode = (poti > 128u);
}

/*###############################################
            Main Loop & Algorithms
#################################################*/

void loop() {
  cli();
  uint8_t poti = readAdcChannel1();
  initAdc0FreeRun();
  setNewSampleRatesForMicAndSpeaker(poti);
  sei();
  _delay_ms(250);
}

uint8_t readAdcChannel1(void)
{
  ADCSRA = 0;                                         // Stop ADC and terminate previous conversion
  ADMUX = (1<<REFS0)|(1<<ADLAR)|(1<<MUX0);            // Vref=Vsup & Left Aligned & ADC1 selected
  ADCSRA = (1<<ADEN)|(1<<ADSC)|(1<<ADPS2)|(1<<ADPS0); // ADC Enable & ADC div 32
  uint8_t running;
  do{
    running = ADCSRA & (1<<ADSC);
  }while(running);
  return ADCH;
}

void setNewSampleRatesForMicAndSpeaker(uint8_t target)
{
  if(target >= 128u)
  {
    OCR0A = TIMER_BASE_DIV;
    if(continuousPitchMode)
    {
      OCR1AL = TIMER_BASE_DIV + ((target - 128u) >> 3u);
    }
    else
    {
      OCR1AL = TIMER_BASE_DIV << 1u;
    }
  }
  else
  {
    if(continuousPitchMode)
    {
      OCR0A = TIMER_BASE_DIV + ((127u - target) >> 3u);
    }
    else
    {
      OCR0A = TIMER_BASE_DIV << 1u;
    }
    OCR1AL = TIMER_BASE_DIV;
  }
}

uint8_t lowPass(uint8_t sample, uint16_t frequency)
{
  static uint8_t last = 128u;
  uint8_t retVal = (uint8_t)(((uint16_t)sample * (uint16_t)frequency + (uint16_t)last * (uint16_t)(FILT_DIV - frequency))/FILT_DIV);
  last = retVal;
  return retVal;
}

/*###############################################
           Interrupt Service Routines
#################################################*/
// Output samples to speaker
ISR(TIMER0_COMPA_vect)
{
  outputSample = lowPass(sampleBuffer[outputPointer], FILT_4kHz);
  outputSample = sampleBuffer[outputPointer];
  outputPointer = (outputPointer + 1u) % PITCH_BUFFER_LEN;
}

// Read Mic at constant sampling rate
ISR(TIMER1_COMPA_vect)
{
  sampleBuffer[inputPointer] = ADCH;
  inputPointer = (inputPointer + 1u) % PITCH_BUFFER_LEN;
}

ISR(TIMER2_OVF_vect)
{
  OCR2B = outputSample;
}
