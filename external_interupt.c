#include <LPC214x.h>

__irq void EINT0_ISR(void)
{
    /* Interrupt processing */

    EXTINT = (1 << 0);

    /* End of interrupt */
    VICVectAddr = 0;
}

void EINT0_Init(void)
{
    /* Configure P0.16 as EINT0 */
    PINSEL1 &= ~(3 << 0);
    PINSEL1 |=  (1 << 0);

    /* Edge-sensitive */
    EXTMODE |= (1 << 0);

    /* Falling edge */
    EXTPOLAR &= ~(1 << 0);

    /* EINT0 interrupt */
    VICIntSelect &= ~(1 << 14);

    VICIntEnable |= (1 << 14);

    /* Assign ISR */
    VICVectAddr0 = (unsigned long)EINT0_ISR;

    /* Enable vector slot 0 and assign source 14 */
    VICVectCntl0 = (1 << 5) | 14;
}

int main(void)
{
    EINT0_Init();

    while(1)
    {
        /* Main application */
    }
}
