#include <LPC214x.h>

#define PCLK 15000000UL

void UART0_Init(unsigned int baud)
{
    unsigned int divisor;

    /* P0.0 = TXD0
       P0.1 = RXD0 */
    PINSEL0 &= ~0x0000000F;
    PINSEL0 |=  0x00000005;

    /* Enable divisor latch */
    U0LCR = 0x83;

    divisor = PCLK / (16 * baud);

    U0DLL = divisor & 0xFF;
    U0DLM = (divisor >> 8) & 0xFF;

    /* 8-bit, 1 stop bit, no parity */
    U0LCR = 0x03;

    /* Enable FIFO */
    U0FCR = 0x07;
}

void UART0_SendChar(char ch)
{
    while(!(U0LSR & (1 << 5)));

    U0THR = ch;
}

char UART0_ReceiveChar(void)
{
    while(!(U0LSR & (1 << 0)));

    return U0RBR;
}

void UART0_SendString(char *str)
{
    while(*str)
    {
        UART0_SendChar(*str++);
    }
}

int main(void)
{
    char ch;

    UART0_Init(9600);

    UART0_SendString("LPC2148 UART Test\r\n");

    while(1)
    {
        ch = UART0_ReceiveChar();

        UART0_SendChar(ch);
    }
}