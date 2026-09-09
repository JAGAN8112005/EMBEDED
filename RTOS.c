#include "FreeRTOS.h"
#include "task.h"

/* Task 1 */
void Task1(void *pvParameters)
{
    while(1)
    {
        printf("Task 1 is running\n");

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* Task 2 */
void Task2(void *pvParameters)
{
    while(1)
    {
        printf("Task 2 is running\n");

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

int main(void)
{
    /* Create Task 1 */
    xTaskCreate(
        Task1,          // Task function
        "Task1",        // Task name
        128,            // Stack size
        NULL,           // Parameter
        1,              // Priority
        NULL            // Task handle
    );

    /* Create Task 2 */
    xTaskCreate(
        Task2,
        "Task2",
        128,
        NULL,
        2,
        NULL
    );

    /* Start RTOS scheduler */
    vTaskStartScheduler();

    while(1);
}
