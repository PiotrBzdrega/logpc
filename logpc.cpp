#include <iostream>
#include "SerialCom.h"          // serial communication
#include "UIHandle.h"           // WinApi




void task2(SerialCom& instance)
{
    printf("tsk2 started\n");
    uint8_t* message = (uint8_t*)"1,adam";

    while (true)
    {
        instance.write_port(message, 6);
 
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

int main()

{//TODO: try to use promise & future
//TODO: add spdlog log library
    //TODO: use [SPACE] or ~ or | [DEL] as separator in telegram
    //TODO: resource management
    //TODO: check continously if web-browser is switched on, to be able immediatelly enter credentials

/* Interface with windows API*/
    UIHandle win;

    /* Serial communication with esp32,
       handle_t try to keep connection up
       through connection_loop() */
    SerialCom esp; 

    /* callback to send telegram*/
    win.add_callback([&esp](uint8_t* buffer, size_t size) {return esp.write_port(buffer, size); });

    /* callback to process received telegram*/
    esp.add_callback([&win](std::string credential) {return win.process_data(credential); });

}
