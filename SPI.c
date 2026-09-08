#include <LPC214x.h>

void SPI_Init(void)
{
    /* P0.4 = SCK
       P0.5 = MOSI
       P0.6 = MISO
       P0.7 = SSEL */

    PINSEL0 &= ~(0x0000FF00);
    PINSEL0 |=  (0x00005500);

    /* Master, 8-bit, CPOL=0, CPHA=0 */
    S0SPCR = (1 << 5);

    /* SPI clock divider */
    S0SPCCR = 8;
}

unsigned char SPI_Transfer(unsigned char data)
{
    S0SPDR = data;

    /* Wait for transfer complete */
    while(!(S0SPSR & (1 << 7)));

    return S0SPDR;
}

int main(void)
{
    unsigned char received;

    SPI_Init();

    while(1)
    {
        received = SPI_Transfer(0x55);
    }
}