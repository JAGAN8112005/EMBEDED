#include <LPC214x.h>

#define CS_PIN 7

void SPI_Init(void)
{
    /* P0.4 = SCK0
       P0.5 = MOSI0
       P0.6 = MISO0 */

    PINSEL0 &= ~(0x3F << 8);
    PINSEL0 |=  (0x15 << 8);

    /* CS as GPIO */
    IODIR0 |= (1 << CS_PIN);
    IOSET0 = (1 << CS_PIN);

    /* SPI Master
       8-bit transfer
       SPI enabled */

    S0SPCR = (1 << 5) | (1 << 4);

    /* SPI clock divider */

    S0SPCCR = 8;
}


unsigned char SPI_Transfer(unsigned char data)
{
    S0SPDR = data;

    while(!(S0SPSR & (1 << 7)));

    return S0SPDR;
}


unsigned int MAX6675_ReadRaw(void)
{
    unsigned char high_byte;
    unsigned char low_byte;
    unsigned int value;

    /* Select MAX6675 */
    IOCLR0 = (1 << CS_PIN);

    high_byte = SPI_Transfer(0x00);
    low_byte  = SPI_Transfer(0x00);

    /* Deselect MAX6675 */
    IOSET0 = (1 << CS_PIN);

    value = ((unsigned int)high_byte << 8) | low_byte;

    return value;
}


float MAX6675_ReadTemperature(void)
{
    unsigned int raw;

    raw = MAX6675_ReadRaw();

    /* Check thermocouple open condition */
    if(raw & (1 << 2))
        return -1;

    /* Temperature bits are D14:D3 */
    raw = raw >> 3;

    /* Each bit represents 0.25 degree Celsius */
    return raw * 0.25;
}


int main(void)
{
    float temperature;

    SPI_Init();

    while(1)
    {
        temperature = MAX6675_ReadTemperature();

        /* Put breakpoint here and observe
           temperature in debugger */

    }
}
