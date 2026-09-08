#include <LPC214x.h>

void ADC_Init(void)
{
    /*
     * Example:
     * P0.28 = AD0.1
     */

    PINSEL1 &= ~(3 << 24);
    PINSEL1 |=  (1 << 24);

    /*
     * Select channel 1
     * ADC clock divider
     * Power-up ADC
     */
    AD0CR = (1 << 1)
          | (4 << 8)
          | (1 << 21);
}

unsigned int ADC_Read(void)
{
    unsigned int value;

    /* Start conversion */
    AD0CR |= (1 << 24);

    /* Wait until conversion complete */
    while(!(AD0DR1 & (1UL << 31)));

    /* Extract 10-bit result */
    value = (AD0DR1 >> 6) & 0x3FF;

    /* Stop conversion */
    AD0CR &= ~(7 << 24);

    return value;
}

int main(void)
{
    unsigned int adc_value;

    ADC_Init();

    while(1)
    {
        adc_value = ADC_Read();

        /*
         * ADC result:
         * 0 - 1023
         */
    }
}
