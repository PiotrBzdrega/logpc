#include <iostream>
#include "SerialCom.h"          //serial communication




void task2(SerialCom& instance)
{
    printf("tsk2 started\n");
    uint8_t* message = (uint8_t*)"1,adam";

    while (true)
    {

        if (instance.write_port(message, 6))
            printf("write message\n");


        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

void create_serial_con()
{
    SerialCom esp;

    std::thread d(task2, std::ref(esp));//start thread (find_url)

    d.join();
}

int main()
{
    create_serial_con();
}
