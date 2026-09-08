#include <LPC214x.h>

void PWM_Init(void)
{
    /*
     * Example PWM pin configuration.
     * Verify the exact pin function from
     * the LPC2148 datasheet before hardware use.
     */

    /* PWM period */
    PWMMR0 = 1000;

    /* Initial duty = 50% */
    PWMMR1 = 500;

    /* Latch MR0 and MR1 */
    PWMLER = (1 << 0) | (1 << 1);

    /* Enable PWM1 output */
    PWMPCR = (1 << 9);

    /* Reset counter on MR0 */
    PWMMCR = (1 << 1);

    /* Enable counter + PWM */
    PWMTCR = (1 << 0) | (1 << 3);
}

void PWM_SetDuty(unsigned int duty)
{
    if(duty > 1000)
        duty = 1000;

    PWMMR1 = duty;

    /* Latch new value */
    PWMLER = (1 << 1);
}

int main(void)
{
    PWM_Init();

    while(1)
    {
        PWM_SetDuty(250);   /* 25% */
    }
}
