#include <LPC214x.h>

#define STA  (1 << 5)
#define STO  (1 << 4)
#define SI   (1 << 3)
#define AA   (1 << 2)
#define I2EN (1 << 6)

void I2C_Init(void)
{
    /* P0.2 = SCL0
       P0.3 = SDA0 */

    PINSEL0 &= ~(0xF0);
    PINSEL0 |=  (0x50);

    /* I2C clock */
    I2SCLH = 75;
    I2SCLL = 75;

    /* Enable I2C */
    I2CONSET = I2EN;
}

unsigned char I2C_Start(void)
{
    I2CONSET = STA;

    I2CONCLR = SI;

    while(!(I2CONSET & SI));

    return I2STAT;
}

void I2C_Stop(void)
{
    I2CONSET = STO;
    I2CONCLR = SI | STA;
}

unsigned char I2C_Write(unsigned char data)
{
    I2DAT = data;

    I2CONCLR = SI;

    while(!(I2CONSET & SI));

    return I2STAT;
}

unsigned char I2C_Read(unsigned char ack)
{
    if(ack)
        I2CONSET = AA;
    else
        I2CONCLR = AA;

    I2CONCLR = SI;

    while(!(I2CONSET & SI));

    return I2DAT;
}

int main(void)
{
    I2C_Init();

    I2C_Start();

    /* Example transaction */
    I2C_Write(0xA0);

    I2C_Write(0x00);

    I2C_Write(0x55);

    I2C_Stop();

    while(1);
}