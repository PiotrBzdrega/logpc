#include <iostream>
#include "SerialCom.h"          // serial communication
#include "UIHandle.h"           // WinApi




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

int main()
{//TODO: try to use promise & future
//TODO: add spdlog log library
    /* Serial communication with esp32*/
    SerialCom esp;

    /* Interface with windows API*/
    UIHandle win;

    /* callback to send telegram*/
    win.add_callback([&esp](uint8_t* buffer, size_t size) {return esp.write_port(buffer, size); });

    /* callback to process received telegram*/
    esp.add_callback([&win](uint8_t* buffer, size_t size) {return win.process_data(buffer, size); });

    std::thread d(task2, std::ref(esp));//start thread (find_url)

    d.join();

    
}
