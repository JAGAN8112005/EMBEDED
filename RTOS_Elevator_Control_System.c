#include <LPC214x.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/*==========================================================
                     PIN DEFINITIONS
==========================================================*/

/* ---------- UART0 ---------- */
/* P0.0 = TXD0
   P0.1 = RXD0 */


/* ---------- LCD ---------- */

#define LCD_D4      16      /* P0.16 */
#define LCD_D5      17      /* P0.17 */
#define LCD_D6      18      /* P0.18 */
#define LCD_D7      19      /* P0.19 */
#define LCD_RS      20      /* P0.20 */
#define LCD_EN      21      /* P0.21 */


/* ---------- Floor sensors ---------- */

#define FLOOR1_PIN  22      /* P0.22 */
#define FLOOR2_PIN  23      /* P0.23 */
#define FLOOR3_PIN  24      /* P1.24 */
#define FLOOR4_PIN  25      /* P1.25 */


/* ---------- Motor driver ---------- */

#define MOTOR_IN1   24      /* P0.24 */
#define MOTOR_IN2   25      /* P0.25 */

/* P0.7 = PWM2 = motor enable */


/* ---------- Servo ---------- */

#define SERVO_PIN   8       /* P0.8 */


/* ---------- Keypad ---------- */

#define ROW0        16      /* P1.16 */
#define ROW1        17      /* P1.17 */
#define ROW2        18      /* P1.18 */
#define ROW3        19      /* P1.19 */

#define COL0        20      /* P1.20 */
#define COL1        21      /* P1.21 */
#define COL2        22      /* P1.22 */
#define COL3        23      /* P1.23 */


/*==========================================================
                       RTOS OBJECTS
==========================================================*/

QueueHandle_t floorQueue;
QueueHandle_t motorQueue;

SemaphoreHandle_t ioMutex;


/*==========================================================
                    GLOBAL VARIABLES
==========================================================*/

typedef enum
{
    STATE_IDLE,
    STATE_DOOR_OPENING,
    STATE_DOOR_OPEN,
    STATE_DOOR_CLOSING,
    STATE_MOVING_UP,
    STATE_MOVING_DOWN,
    STATE_STOPPED

} ElevatorState;


typedef enum
{
    MOTOR_STOP,
    MOTOR_UP,
    MOTOR_DOWN

} MotorCommand;


volatile ElevatorState elevatorState = STATE_IDLE;

volatile uint8_t currentFloor = 1;
volatile uint8_t targetFloor  = 1;


/*==========================================================
                     CLOCK INITIALIZATION
==========================================================*/

/*
   Assumption:
   External crystal = 12 MHz

   PLL:
   M = 5
   P = 2

   CCLK = 60 MHz

   VPBDIV = 0
   Therefore:

   PCLK = CCLK / 4
        = 15 MHz
*/

void Clock_Init(void)
{
    /* Enable PLL */

    PLL0CON = 0x01;

    /* M = 5
       P = 2 */

    PLL0CFG = 0x24;

    PLL0FEED = 0xAA;
    PLL0FEED = 0x55;

    /* Wait for PLL lock */

    while(!(PLL0STAT & (1 << 10)));

    /* Connect PLL */

    PLL0CON = 0x03;

    PLL0FEED = 0xAA;
    PLL0FEED = 0x55;

    /*
       PCLK = CCLK / 4
    */

    VPBDIV = 0x00;
}


/*==========================================================
                     TIMER0 MICROSECOND TIMER
==========================================================*/

void Timer0_Init(void)
{
    /*
       PCLK = 15 MHz

       We want:

       1 timer count = 1 us

       PR + 1 = 15

       PR = 14
    */

    T0CTCR = 0x00;

    T0PR = 14;

    T0TCR = 0x02;

    T0TCR = 0x01;
}


void Delay_us(uint32_t us)
{
    T0TCR = 0x02;       /* Reset */

    T0TCR = 0x01;       /* Enable */

    T0TC = 0;

    while(T0TC < us);

    T0TCR = 0x00;       /* Disable */
}


/*==========================================================
                         UART0
==========================================================*/

void UART0_Init(void)
{
    /*
       P0.0 -> TXD0
       P0.1 -> RXD0
    */

    PINSEL0 &= ~(0x0F);

    PINSEL0 |= 0x05;


    /*
       8-bit
       1 stop bit
       No parity
       DLAB = 1
    */

    U0LCR = 0x83;


    /*
       PCLK = 15 MHz

       Approximate divisor for 9600 baud.

       Baud = PCLK / (16 * divisor)

       divisor ≈ 97
    */

    U0DLL = 97;
    U0DLM = 0;


    /*
       DLAB = 0

       8N1 remains enabled
    */

    U0LCR = 0x03;


    /*
       Enable FIFO
       Reset RX FIFO
       Reset TX FIFO
    */

    U0FCR = 0x07;
}


void UART0_SendChar(char c)
{
    while(!(U0LSR & (1 << 5)));

    U0THR = c;
}


void UART0_SendString(const char *str)
{
    while(*str)
    {
        UART0_SendChar(*str);

        str++;
    }
}


/*==========================================================
                         LCD DRIVER
==========================================================*/

void LCD_Delay(void)
{
    Delay_us(2000);
}


void LCD_EnablePulse(void)
{
    IO0SET = (1 << LCD_EN);

    Delay_us(10);

    IO0CLR = (1 << LCD_EN);

    Delay_us(100);
}


void LCD_SendNibble(uint8_t data)
{
    /* Clear D4-D7 */

    IO0CLR =
        (1 << LCD_D4) |
        (1 << LCD_D5) |
        (1 << LCD_D6) |
        (1 << LCD_D7);


    /* Put data on D4-D7 */

    if(data & 0x01)
        IO0SET = (1 << LCD_D4);

    if(data & 0x02)
        IO0SET = (1 << LCD_D5);

    if(data & 0x04)
        IO0SET = (1 << LCD_D6);

    if(data & 0x08)
        IO0SET = (1 << LCD_D7);


    LCD_EnablePulse();
}


void LCD_Command(uint8_t command)
{
    IO0CLR = (1 << LCD_RS);

    LCD_SendNibble(command >> 4);

    LCD_SendNibble(command & 0x0F);

    LCD_Delay();
}


void LCD_Data(uint8_t data)
{
    IO0SET = (1 << LCD_RS);

    LCD_SendNibble(data >> 4);

    LCD_SendNibble(data & 0x0F);

    LCD_Delay();
}


void LCD_Init(void)
{
    /*
       P0.16-P0.21 as GPIO
    */

    IO0DIR |=
        (1 << LCD_D4) |
        (1 << LCD_D5) |
        (1 << LCD_D6) |
        (1 << LCD_D7) |
        (1 << LCD_RS) |
        (1 << LCD_EN);


    IO0CLR =
        (1 << LCD_D4) |
        (1 << LCD_D5) |
        (1 << LCD_D6) |
        (1 << LCD_D7) |
        (1 << LCD_RS) |
        (1 << LCD_EN);


    Delay_us(20000);


    /*
       LCD initialization sequence
    */

    LCD_SendNibble(0x03);

    Delay_us(5000);

    LCD_SendNibble(0x03);

    Delay_us(200);

    LCD_SendNibble(0x03);

    LCD_SendNibble(0x02);


    /*
       4-bit mode
       2-line display
       5x8 font
    */

    LCD_Command(0x28);

    /*
       Display ON
       Cursor OFF
    */

    LCD_Command(0x0C);

    /*
       Entry mode
    */

    LCD_Command(0x06);

    /*
       Clear display
    */

    LCD_Command(0x01);
}


void LCD_Clear(void)
{
    LCD_Command(0x01);
}


void LCD_SetCursor(uint8_t row, uint8_t column)
{
    uint8_t address;

    if(row == 0)
        address = 0x80 + column;

    else
        address = 0xC0 + column;

    LCD_Command(address);
}


void LCD_Print(const char *str)
{
    while(*str)
    {
        LCD_Data(*str);

        str++;
    }
}


/*==========================================================
                       KEYPAD DRIVER
==========================================================*/

void Keypad_Init(void)
{
    /*
       Rows = output
       Columns = input
    */

    IO1DIR |=
        (1 << ROW0) |
        (1 << ROW1) |
        (1 << ROW2) |
        (1 << ROW3);


    IO1DIR &=
        ~((1 << COL0) |
          (1 << COL1) |
          (1 << COL2) |
          (1 << COL3));


    /*
       Rows initially HIGH
    */

    IO1SET =
        (1 << ROW0) |
        (1 << ROW1) |
        (1 << ROW2) |
        (1 << ROW3);
}


char Keypad_GetKey(void)
{
    const char keyMap[4][4] =
    {
        {'1','2','3','A'},
        {'4','5','6','B'},
        {'7','8','9','C'},
        {'*','0','#','D'}
    };

    uint8_t row;

    uint32_t rows[4] =
    {
        ROW0,
        ROW1,
        ROW2,
        ROW3
    };

    uint32_t cols[4] =
    {
        COL0,
        COL1,
        COL2,
        COL3
    };


    for(row = 0; row < 4; row++)
    {
        /*
           Set all rows HIGH
        */

        IO1SET =
            (1 << ROW0) |
            (1 << ROW1) |
            (1 << ROW2) |
            (1 << ROW3);


        /*
           Current row LOW
        */

        IO1CLR = (1 << rows[row]);


        Delay_us(100);


        /*
           Check columns
        */

        if(!(IO1PIN & (1 << cols[0])))
            return keyMap[row][0];

        if(!(IO1PIN & (1 << cols[1])))
            return keyMap[row][1];

        if(!(IO1PIN & (1 << cols[2])))
            return keyMap[row][2];

        if(!(IO1PIN & (1 << cols[3])))
            return keyMap[row][3];
    }

    return 0;
}


/*==========================================================
                      FLOOR SENSOR DRIVER
==========================================================*/

void FloorSensor_Init(void)
{
    /*
       Floor sensors are inputs.
    */

    IO0DIR &=
        ~((1 << FLOOR1_PIN) |
          (1 << FLOOR2_PIN));


    IO1DIR &=
        ~((1 << FLOOR3_PIN) |
          (1 << FLOOR4_PIN));
}


uint8_t FloorSensor_Read(void)
{
    /*
       Assumption:
       Sensor becomes LOW when active.

       Change this logic if your sensor is active HIGH.
    */

    if(!(IO0PIN & (1 << FLOOR1_PIN)))
        return 1;

    if(!(IO0PIN & (1 << FLOOR2_PIN)))
        return 2;

    if(!(IO1PIN & (1 << FLOOR3_PIN)))
        return 3;

    if(!(IO1PIN & (1 << FLOOR4_PIN)))
        return 4;

    return 0;
}


/*==========================================================
                         MOTOR PWM
==========================================================*/

/*
       P0.7 = PWM2
*/

void Motor_PWM_Init(void)
{
    /*
       P0.7 -> PWM2

       PINSEL0 bits 15:14 = 10
    */

    PINSEL0 &= ~(3 << 14);

    PINSEL0 |= (2 << 14);


    /*
       Motor direction pins
    */

    IO0DIR |=
        (1 << MOTOR_IN1) |
        (1 << MOTOR_IN2);


    /*
       20 kHz PWM

       PCLK = 15 MHz

       PWMPR = 14

       PWM timer frequency:

       15MHz / 15 = 1MHz

       Therefore:

       1 count = 1us

       MR0 = 50

       Period = 50us

       Frequency = 20kHz
    */

    PWMPR = 14;

    PWMMR0 = 50;

    PWMMR2 = 0;


    /*
       Reset PWM counter on MR0
    */

    PWMMCR = (1 << 1);


    /*
       Latch MR0 and MR2
    */

    PWMLER =
        (1 << 0) |
        (1 << 2);


    /*
       Enable PWM2
    */

    PWMPCR = (1 << 10);


    /*
       Reset PWM timer
    */

    PWMTCR = (1 << 1);


    /*
       Enable PWM counter
       Enable PWM mode
    */

    PWMTCR =
        (1 << 0) |
        (1 << 3);
}


void Motor_SetSpeed(uint8_t duty)
{
    if(duty > 100)
        duty = 100;


    /*
       MR0 = 50

       duty = 0 to 100
    */

    PWMMR2 = (50 * duty) / 100;

    PWMLER = (1 << 2);
}


void Motor_Forward(void)
{
    IO0CLR = (1 << MOTOR_IN2);

    IO0SET = (1 << MOTOR_IN1);

    Motor_SetSpeed(70);
}


void Motor_Reverse(void)
{
    IO0CLR = (1 << MOTOR_IN1);

    IO0SET = (1 << MOTOR_IN2);

    Motor_SetSpeed(70);
}


void Motor_Stop(void)
{
    IO0CLR =
        (1 << MOTOR_IN1) |
        (1 << MOTOR_IN2);

    Motor_SetSpeed(0);
}


/*==========================================================
                       SERVO DRIVER
==========================================================*/

/*
   Servo signal = P0.8

   Software-generated 50Hz signal.

   Period = 20ms

   Open  = approximately 2ms pulse
   Close = approximately 1ms pulse
*/

void Servo_Init(void)
{
    /*
       P0.8 as GPIO
    */

    PINSEL0 &= ~(3 << 16);

    IO0DIR |= (1 << SERVO_PIN);

    IO0CLR = (1 << SERVO_PIN);
}


void Servo_Pulse(uint32_t pulse_us)
{
    IO0SET = (1 << SERVO_PIN);

    Delay_us(pulse_us);

    IO0CLR = (1 << SERVO_PIN);

    Delay_us(20000 - pulse_us);
}


void Servo_Open(void)
{
    /*
       Send several pulses so the servo reaches position.
    */

    int i;

    for(i = 0; i < 10; i++)
        Servo_Pulse(2000);
}


void Servo_Close(void)
{
    int i;

    for(i = 0; i < 10; i++)
        Servo_Pulse(1000);
}


/*==========================================================
                  ELEVATOR REQUEST PROCESSING
==========================================================*/

void ProcessFloorRequest(uint8_t requestedFloor)
{
    MotorCommand command;


    if(requestedFloor < 1 || requestedFloor > 4)
        return;


    targetFloor = requestedFloor;


    /*
       Already at requested floor
    */

    if(targetFloor == currentFloor)
    {
        elevatorState = STATE_DOOR_OPENING;

        Servo_Open();

        elevatorState = STATE_DOOR_OPEN;

        return;
    }


    /*
       Target is above current floor
    */

    if(targetFloor > currentFloor)
    {
        elevatorState = STATE_DOOR_CLOSING;

        Servo_Close();

        command = MOTOR_UP;

        xQueueSend(
            motorQueue,
            &command,
            portMAX_DELAY
        );

        elevatorState = STATE_MOVING_UP;


        /*
           Wait until target floor is detected.
        */

        while(currentFloor != targetFloor)
        {
            uint8_t floor;

            floor = FloorSensor_Read();

            if(floor != 0)
            {
                currentFloor = floor;
            }

            vTaskDelay(pdMS_TO_TICKS(50));
        }


        command = MOTOR_STOP;

        xQueueSend(
            motorQueue,
            &command,
            portMAX_DELAY
        );


        elevatorState = STATE_STOPPED;

        vTaskDelay(pdMS_TO_TICKS(100));


        elevatorState = STATE_DOOR_OPENING;

        Servo_Open();

        elevatorState = STATE_DOOR_OPEN;

        vTaskDelay(pdMS_TO_TICKS(3000));

        elevatorState = STATE_DOOR_CLOSING;

        Servo_Close();

        elevatorState = STATE_IDLE;

        return;
    }


    /*
       Target is below current floor
    */

    if(targetFloor < currentFloor)
    {
        elevatorState = STATE_DOOR_CLOSING;

        Servo_Close();

        command = MOTOR_DOWN;

        xQueueSend(
            motorQueue,
            &command,
            portMAX_DELAY
        );

        elevatorState = STATE_MOVING_DOWN;


        while(currentFloor != targetFloor)
        {
            uint8_t floor;

            floor = FloorSensor_Read();

            if(floor != 0)
            {
                currentFloor = floor;
            }

            vTaskDelay(pdMS_TO_TICKS(50));
        }


        command = MOTOR_STOP;

        xQueueSend(
            motorQueue,
            &command,
            portMAX_DELAY
        );


        elevatorState = STATE_STOPPED;

        vTaskDelay(pdMS_TO_TICKS(100));


        elevatorState = STATE_DOOR_OPENING;

        Servo_Open();

        elevatorState = STATE_DOOR_OPEN;

        vTaskDelay(pdMS_TO_TICKS(3000));

        elevatorState = STATE_DOOR_CLOSING;

        Servo_Close();

        elevatorState = STATE_IDLE;
    }
}


/*==========================================================
                         KEYPAD TASK
==========================================================*/

void KeypadTask(void *pvParameters)
{
    char key;
    uint8_t floor;

    while(1)
    {
        key = Keypad_GetKey();

        if(key >= '1' && key <= '4')
        {
            floor = key - '0';

            xQueueSend(
                floorQueue,
                &floor,
                portMAX_DELAY
            );

            /*
               Wait for key release/debounce.
            */

            vTaskDelay(
                pdMS_TO_TICKS(200)
            );
        }

        vTaskDelay(
            pdMS_TO_TICKS(50)
        );
    }
}


/*==========================================================
                         MOTOR TASK
==========================================================*/

void MotorTask(void *pvParameters)
{
    MotorCommand command;

    while(1)
    {
        if(xQueueReceive(
            motorQueue,
            &command,
            portMAX_DELAY))
        {
            switch(command)
            {
                case MOTOR_UP:

                    Motor_Forward();

                    break;


                case MOTOR_DOWN:

                    Motor_Reverse();

                    break;


                case MOTOR_STOP:

                    Motor_Stop();

                    break;


                default:

                    Motor_Stop();

                    break;
            }
        }
    }
}


/*==========================================================
                       ELEVATOR TASK
==========================================================*/

void ElevatorTask(void *pvParameters)
{
    uint8_t requestedFloor;

    while(1)
    {
        if(xQueueReceive(
            floorQueue,
            &requestedFloor,
            portMAX_DELAY))
        {
            ProcessFloorRequest(
                requestedFloor
            );
        }

        vTaskDelay(
            pdMS_TO_TICKS(10)
        );
    }
}


/*==========================================================
                       UART TASK
==========================================================*/

void UARTTask(void *pvParameters)
{
    char buffer[80];

    while(1)
    {
        if(xSemaphoreTake(
            ioMutex,
            portMAX_DELAY))
        {
            UART0_SendString("\r\n");
            UART0_SendString("====================\r\n");

            UART0_SendString("ELEVATOR STATUS\r\n");

            UART0_SendString("====================\r\n");

            UART0_SendString("Current Floor: ");

            UART0_SendChar(
                '0' + currentFloor
            );

            UART0_SendString("\r\n");


            UART0_SendString("Target Floor : ");

            UART0_SendChar(
                '0' + targetFloor
            );

            UART0_SendString("\r\n");


            UART0_SendString("State        : ");

            switch(elevatorState)
            {
                case STATE_IDLE:
                    UART0_SendString("IDLE");
                    break;

                case STATE_DOOR_OPENING:
                    UART0_SendString("DOOR OPENING");
                    break;

                case STATE_DOOR_OPEN:
                    UART0_SendString("DOOR OPEN");
                    break;

                case STATE_DOOR_CLOSING:
                    UART0_SendString("DOOR CLOSING");
                    break;

                case STATE_MOVING_UP:
                    UART0_SendString("MOVING UP");
                    break;

                case STATE_MOVING_DOWN:
                    UART0_SendString("MOVING DOWN");
                    break;

                case STATE_STOPPED:
                    UART0_SendString("STOPPED");
                    break;

                default:
                    UART0_SendString("UNKNOWN");
                    break;
            }

            UART0_SendString("\r\n");

            UART0_SendString("====================\r\n");

            xSemaphoreGive(ioMutex);
        }

        vTaskDelay(
            pdMS_TO_TICKS(1000)
        );
    }
}


/*==========================================================
                        LCD TASK
==========================================================*/

void LCDTask(void *pvParameters)
{
    while(1)
    {
        if(xSemaphoreTake(
            ioMutex,
            portMAX_DELAY))
        {
            LCD_Clear();

            LCD_SetCursor(0,0);

            LCD_Print("Floor:");

            LCD_Data(
                '0' + currentFloor
            );


            LCD_SetCursor(0,9);

            LCD_Print("Target:");

            LCD_Data(
                '0' + targetFloor
            );


            LCD_SetCursor(1,0);

            switch(elevatorState)
            {
                case STATE_IDLE:
                    LCD_Print("IDLE");
                    break;

                case STATE_MOVING_UP:
                    LCD_Print("MOVING UP");
                    break;

                case STATE_MOVING_DOWN:
                    LCD_Print("MOVING DOWN");
                    break;

                case STATE_DOOR_OPEN:
                    LCD_Print("DOOR OPEN");
                    break;

                case STATE_DOOR_CLOSING:
                    LCD_Print("DOOR CLOSE");
                    break;

                case STATE_STOPPED:
                    LCD_Print("STOPPED");
                    break;

                default:
                    LCD_Print("WORKING");
                    break;
            }

            xSemaphoreGive(ioMutex);
        }

        vTaskDelay(
            pdMS_TO_TICKS(250)
        );
    }
}


/*==========================================================
                         MAIN
==========================================================*/

int main(void)
{
    /*
       --------------------------------
       1. Clock
       --------------------------------
    */

    Clock_Init();


    /*
       --------------------------------
       2. Timer
       --------------------------------
    */

    Timer0_Init();


    /*
       --------------------------------
       3. GPIO / Peripheral Drivers
       --------------------------------
    */

    UART0_Init();

    LCD_Init();

    Keypad_Init();

    FloorSensor_Init();

    Motor_PWM_Init();

    Servo_Init();


    /*
       --------------------------------
       4. Create Queues
       --------------------------------
    */

    /*
       Floor request queue

       Stores requested floors
    */

    floorQueue =
        xQueueCreate(
            5,
            sizeof(uint8_t)
        );


    /*
       Motor command queue

       Stores MOTOR_UP,
       MOTOR_DOWN,
       MOTOR_STOP
    */

    motorQueue =
        xQueueCreate(
            5,
            sizeof(MotorCommand)
        );


    /*
       --------------------------------
       5. Create Mutex
       --------------------------------
    */

    ioMutex =
        xSemaphoreCreateMutex();


    /*
       --------------------------------
       6. Create Tasks
       --------------------------------
    */

    xTaskCreate(
        KeypadTask,
        "KEYPAD",
        256,
        NULL,
        2,
        NULL
    );


    xTaskCreate(
        ElevatorTask,
        "ELEVATOR",
        512,
        NULL,
        4,
        NULL
    );


    xTaskCreate(
        MotorTask,
        "MOTOR",
        256,
        NULL,
        3,
        NULL
    );


    xTaskCreate(
        UARTTask,
        "UART",
        256,
        NULL,
        1,
        NULL
    );


    xTaskCreate(
        LCDTask,
        "LCD",
        256,
        NULL,
        1,
        NULL
    );


    /*
       --------------------------------
       7. Start Scheduler
       --------------------------------
    */

    vTaskStartScheduler();


    /*
       Scheduler should never return.
    */

    while(1);
}
