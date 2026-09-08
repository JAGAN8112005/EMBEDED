#include <LPC214x.h>

#define PCLK 15000000UL

void Timer0_Init(void)
{
    /* Timer counter increments every 1 ms */
    T0PR = (PCLK / 1000) - 1;

    /* Match after 1000 ms */
    T0MR0 = 1000;

    /* Interrupt + reset on match */
    T0MCR = (1 << 0) | (1 << 1);

    /* Reset timer */
    T0TCR = (1 << 1);

    T0TCR = 0;
}

void Timer0_Start(void)
{
    T0TCR = 1;
}

void Timer0_Stop(void)
{
    T0TCR = 0;
}

int main(void)
{
    Timer0_Init();

    Timer0_Start();

    while(1)
    {
        /* Timer running */
    }
}