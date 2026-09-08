#include <LPC214x.h>

void delay(void)
{
    unsigned int i, j;

    for(i = 0; i < 500; i++)
        for(j = 0; j < 6000; j++);
}

int main(void)
{
    /* P0.10 as GPIO */
    PINSEL0 &= ~(3 << 20);

    /* P0.10 as output */
    IODIR0 |= (1 << 10);

    while(1)
    {
        /* LED ON */
        IOSET0 = (1 << 10);
        delay();

        /* LED OFF */
        IOCLR0 = (1 << 10);
        delay();
    }
}