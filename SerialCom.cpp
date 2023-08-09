#include "SerialCom.h"

SerialCom::SerialCom(const char* device_name, uint32_t com_speed) : device{ device_name }, baud_rate{ com_speed }, port{ nullptr }, old_dwModemStatus{}
{
    /* start port handling thread */
    handle_t = std::thread(&SerialCom::connection_loop, this);
}

SerialCom::~SerialCom()
{
    /* stop port handling thread */
    if (handle_t.joinable())
        handle_t.join();

    /* stop event thread */
    if (event_t.joinable())
        event_t.join();

    /* stop UIHandle callback thread */
    if (callback_t.joinable())
        callback_t.join();

    close_handle();
}

void SerialCom::close_handle()
{
    if (!handle_closed)
    {
        if (!CloseHandle(port))
            print_error("Fault when Closing main Handle");
        handle_closed = true;
    }
    else
        printf("handle already closed\n");
}

bool SerialCom::add_callback(UIHandleCb callback)
{
    /* assign to class callback*/
    this->callback = callback;
    return true;
}

void SerialCom::print_error(const char* context)
{
    DWORD error_code = GetLastError();
    char buffer[256];
    DWORD size = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_MAX_WIDTH_MASK,
        NULL, error_code, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
        buffer, sizeof(buffer), NULL);
    if (size == 0) { buffer[0] = 0; }
    fprintf(stderr, "%s: %s\n", context, buffer);
}

void SerialCom::print_timestamp(const char* tag)
{
    struct tm newtime;
    __time32_t aclock;
    _time32(&aclock);   // Get time in seconds.
    _localtime32_s(&newtime, &aclock);   // Convert time to struct tm form.
    char buffer[26];

    /*Converts given calendar time tm to a textual representation 
    of the following fixed 25-character form: Www Mmm dd hh:mm:ss yyyy\n*/
    asctime_s(buffer, sizeof buffer, &newtime); 
    printf("[%s]The current local time is: %s", tag, buffer);
    return;
}

BOOL SerialCom::withdraw_buffer(SSIZE_T len)
{
    printf("received telegram:%s \t with size:%d\n", (char*)read_buffer,len);

    /* allocate memory for new telegram +1 for null terminator*/
    uint8_t* credential = (uint8_t*)malloc(len+1);

    /* copy all telegram from buffer*/
    for (SSIZE_T i = 0; i < len; i++)
        credential[i] = read_buffer[i];
        
    /* null terminator in case of printf*/
    credential[len] = '\0';

    /* stop UIHandle callback thread if pending */
    if (callback_t.joinable())
        callback_t.join();

    /* start thread for UIHandle thread*/
    callback_t = std::thread(callback, credential, static_cast<int>(len));

    return true;
}

BOOL SerialCom::comm_status(DWORD state)
{
    DWORD dwModemStatus=0;
    bool newModemStatus = false;
    
    //TODO: recognize disconnection faster
    DCB com_state = { 0 };
    if (!GetCommState(port, &com_state))
    {
        print_error("Failed to get serial settings");
        //close_handle();
    }
    else
    {
        if (!SetCommState(port, &com_state))
        {
            print_error("Failed to set serial settings");
        }
        
        //printf("correctly get comm");
    }

    {
        std::lock_guard<decltype(mtx)> lk(mtx);

        if (!GetCommModemStatus(port, &dwModemStatus))
        {
            // Error in GetCommModemStatus;
            print_error("GetCommModemStatus() error");
            return false;
        }       
        if (old_dwModemStatus != dwModemStatus)
        {
            printf("new modem status 0x%04X \n", dwModemStatus);
            newModemStatus = true;
        }
        old_dwModemStatus = dwModemStatus;
        if (!(dwModemStatus & (MS_CTS_ON | MS_DSR_ON | MS_RLSD_ON | MS_RING_ON)))
        {
            //print_error("mutex locked\n");
            if (!port_down)
            {
                printf("0x%04X \n", dwModemStatus);
                printf("port_down\n");
                port_down = true;
                cv.notify_all();
                //close_handle();
                return false;
            }         
        }
        else
        {
            if (port_down)
            {
                //printf("mutex released\n");
                printf("port_up\n");
                port_down = false;
                cv.notify_all();
            }
        }
    }

    if ((state & EV_CTS) || newModemStatus)
    {
        printf("CTS changed state value:%d\n", bool(dwModemStatus & MS_CTS_ON));
    }

    if ((state & EV_DSR) || newModemStatus)
    {
        printf("DSR changed state value:%d\n", bool(dwModemStatus & MS_DSR_ON));
    }

    if ((state & EV_RLSD) || newModemStatus)
    {
        /*RLSD (Receive Line Signal Detect) is commonly referred to
         as the CD (Carrier Detect) line*/
        printf("RLSD changed state value:%d\n", bool(dwModemStatus & MS_RLSD_ON));
    }

    if ((state & EV_RING) || newModemStatus)
    {
        printf("Ring signal detected value:%d\n", bool(dwModemStatus & MS_RING_ON));
    }


    if (state & EV_RXCHAR)
    {
        printf("Any Character received\n");
        SSIZE_T received= read_port();
        if (received)
        {
            withdraw_buffer(received);
        }
    }

    if (state & EV_RXFLAG)
    {
        printf("Received certain character\n");
    }

    if (state & EV_TXEMPTY)
    {
        //TODO: why there is no information about sended telegram???
        printf("Transmitt Queue Empty\n");
    }

    if (state & EV_BREAK)
    {
        printf("BREAK received\n");
    }

    if (state & EV_ERR)
    {
        printf("Line status error occurred\n");
    }

    if (state & EV_PERR)
    {
        printf("Printer error occured\n");
    }

    if (state & EV_RX80FULL)
    {
        printf("Receive buffer is 80 percent full\n");
    }

    if (state & EV_EVENT1)
    {
        printf("Provider specific event 1\n");
    }

    if (state & EV_EVENT2)
    {
        printf("Provider specific event 2\n");
    }

    print_timestamp("comm_status");

    return (state & 0xFFFF);
}

int SerialCom::open_serial_port()
{
    /* cannot create handle again*/
    if (!handle_closed)
        return -1;

    port = CreateFileA(device,                  /* The name of the file or device*/
        GENERIC_READ | GENERIC_WRITE,           /* The requested access to the file or device*/
        0,                                      /* The requested sharing mode of the file or device*/
        0,                                      /* A pointer to a SECURITY_ATTRIBUTES*/
        OPEN_EXISTING,                          /* An action to take on a file or device that exists or does not exist.*/
        FILE_FLAG_OVERLAPPED,                   /* The file or device attributes and flags FILE_ATTRIBUTE_NORMAL*/
        0);                                     /* A valid handle to a template file with the GENERIC_READ access right*/
    if (port == INVALID_HANDLE_VALUE)
    {
        print_error(device);

        return -1;
    }
    else
    {
        /* handle is up and running*/
        handle_closed = false;


        if (event_t.joinable())
        {
            printf("event callback wait for join\n");
            event_t.join();       
        }
           

        /* start event thread */
        event_t = std::thread(&SerialCom::event_callback, this);
    }
    printf("port opened succesfully\n");
    // Flush away any bytes previously read or written.
    BOOL success = FlushFileBuffers(port);
    if (!success)
    {
        print_error("Failed to flush serial port");
        close_handle();
        return -1;
    }

    // Configure read and write operations to time out after 100 ms.
    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = 0;
    timeouts.ReadTotalTimeoutConstant = 100;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    timeouts.WriteTotalTimeoutConstant = 100;
    timeouts.WriteTotalTimeoutMultiplier = 0;

    success = SetCommTimeouts(port, &timeouts);
    if (!success)
    {
        print_error("Failed to set serial timeouts");
        close_handle();
        return -1;
    }

    // Set the baud rate and other options.
    DCB state = { 0 };
    state.DCBlength = sizeof(DCB);
    state.BaudRate = baud_rate;
    state.ByteSize = 8;
    state.Parity = NOPARITY;
    state.StopBits = ONESTOPBIT;
    success = SetCommState(port, &state);
    if (!success)
    {
        print_error("Failed to set serial settings");
        close_handle();
        return -1;
    }

    success = GetCommState(port, &state);
    if (!success)
    {
        print_error("Failed to get serial settings");
        close_handle();
        return -1;
    }

    return 0;
}

void SerialCom::connection_loop()
{
    while (1)
    {
        /* keep opening port*/
        open_serial_port();

        std::this_thread::sleep_for(std::chrono::seconds(5));
        /* till port is down*/
        std::unique_lock<decltype(mtx)> lk(mtx);
        cv.wait(lk, [this]() {return port_down; });
        
        close_handle();

    }
}

void SerialCom::write_port(uint8_t* buffer, size_t size)
{
    {
        std::unique_lock<decltype(mtx)> lk(mtx);
        /* drop writing if mutex got time-out*/
        if (cv.wait_for(lk, std::chrono::seconds(2), [this]() {return !port_down; }))
            printf("write_port| mutex released");
        else
        {
            printf("write_port| mutex blocked");
            return;
        }
    }
    
    /* waits till port is reachable, till this time
       write is blocked*/
    //while (port_down) {
        
    //}
        //TODO: return with fault after some time or event



    /* handle for write operation*/
    OVERLAPPED oWrite={0};
    DWORD written;
    BOOL res=false;
    bool bwait = false;

    oWrite.hEvent = CreateEvent(
        NULL,   // default security attributes 
        TRUE,   // manual-reset event 
        FALSE,  // not signaled 
        NULL    // no name
    );
    if (!oWrite.hEvent)
    {
        print_error("Create event failed");
        return;
    }

    BOOL success = WriteFile(port, buffer, size, NULL, &oWrite);
    if (!success)
    {   
        if (GetLastError() != ERROR_IO_PENDING)
        {
            print_error("Failed to write to port");
            res=false ;
        }
        else
        {
            /* write is pending*/

            /* wait infinite time*/
            DWORD dwRes = WaitForSingleObject(oWrite.hEvent,INFINITE);

            switch (dwRes)
            {
                /*  OVERLAPPED structure's event has been signaled.*/
                case WAIT_OBJECT_0:
                    success = GetOverlappedResult(port, &oWrite, &written, bwait);
                    if (!success)
                        res = false;
                    else
                    {
                        res = true;
                        if (written != size)
                        {
                            printf("written:%lu, size:%zu\n", written, size);
                            print_error("Failed to write all bytes to port ");
                            res = false;
                        }               
                        else
                        {
                            printf("succesfully written:%lu, size:%zu\n", written, size);
                        }
                    }
                break;
            default:
                print_error("Problem with OVERLAPPED event handle ");
                res = false;
                break;
            }
        }        
    }

    if (!CloseHandle(oWrite.hEvent))
        print_error("Fault when Closing Handle oEvent.hEvent");

    return;
}

SSIZE_T SerialCom::read_port()
{
    #define READ_TIMEOUT      500      // milliseconds
    DWORD received=0;
    SSIZE_T res = -1;
    BOOL waiting_on_read = FALSE;
    /* handle for read operation*/
    OVERLAPPED oRead = { 0 };
    bool bwait = false;

    oRead.hEvent = CreateEvent(
    NULL,   // default security attributes 
    TRUE,   // manual-reset event 
    FALSE,  // not signaled 
    NULL    // no name
    );

    if (!oRead.hEvent)
    {
        print_error("Create event failed");
        return -1;
    }

    if (!waiting_on_read)
    {
        BOOL success = ReadFile(port, read_buffer, BUFFER_LEN, &received, &oRead);
        if (!success)
        {
            if (GetLastError() != ERROR_IO_PENDING)
            {
                print_error("Failed to write to port");
                res = -1;
            }
            else
            {
                /* read pending*/
                waiting_on_read = TRUE;
            }
        }
        else
        {
            /* read immediatelly done*/
            print_timestamp("read immediatelly");
        }
    }
    
    if (waiting_on_read)
    {
        DWORD dwRead;
        DWORD dwRes;
        dwRes = WaitForSingleObject(oRead.hEvent, READ_TIMEOUT);
        BOOL success;

        switch (dwRes)
        {
            case WAIT_OBJECT_0:

                success = GetOverlappedResult(port, &oRead, &received, bwait);
                if (!success)
                {
                    print_error("Failed to read from port");
                    return -1;
                }
                else
                {
                    /* read immediatelly done*/
                    
                    print_timestamp("read after waits");
                }
                waiting_on_read = false;

            case WAIT_TIMEOUT:
                // Operation isn't complete yet. fWaitingOnRead flag isn't
                // changed since I'll loop back around, and I don't want
                // to issue another read until the first one finishes.
                break;

            default:
                // Error in the WaitForSingleObject; abort.
                // This indicates a problem with the OVERLAPPED structure's
                // event handle.
                break;
        }
    }

    if (!CloseHandle(oRead.hEvent))
        print_error("Fault when Closing Handle oRead.hEvent");

    return received;
}

void SerialCom::event_callback() 
{
    
      
    //using namespace std::literals::chrono_literals;
    DWORD  dwEvtMask=0;
    BOOL success;
    BOOL fWaitingOnStat = FALSE;
    #define STATUS_CHECK_TIMEOUT 500 // Milliseconds


    /* allow all events to be detected */
    dwEvtMask = EV_RXCHAR |
        EV_RXFLAG |
        EV_TXEMPTY |
        EV_CTS |
        EV_DSR |
        EV_RLSD |
        EV_BREAK |
        EV_ERR |
        EV_RING |
        EV_PERR |
        EV_RX80FULL |
        EV_EVENT1 |
        EV_EVENT2;

    success = SetCommMask(port, dwEvtMask);
    if (!success)
    {
        print_error("Failed to set communication mask");
        return;
    }

    dwEvtMask = 0;

    success = GetCommMask(port, &dwEvtMask);
    if (!success)
    {
        print_error("Failed to read communication mask");
        return;
    }
    printf("GetCommMask result: 0x%04X \n", dwEvtMask);

    /* handle for write operation*/
    OVERLAPPED oEvent = { 0 };
    oEvent.hEvent = CreateEvent(
        NULL,   // default security attributes 
        TRUE,   // manual-reset event 
        FALSE,  // not signaled 
        NULL    // no name
    );
    if (!oEvent.hEvent)
    {
        print_error("Create event failed");
        return;
    }

    while (1)
    {

        if (!fWaitingOnStat)
        {
            if (WaitCommEvent(port, &dwEvtMask, &oEvent))
            {
                // leave event function if port is down
                if (!comm_status(dwEvtMask))
                {
                    printf("returned from eventcall");
                    return;
                }
                    
            }
            else
            {
                if (GetLastError() != ERROR_IO_PENDING)
                {
                    print_error("communication error");
                    break;
                }
                else
                {
                    fWaitingOnStat = TRUE;
                }
            }
        }
        else
        {
            DWORD dwOvRes;
            /*  Check on overlapped operation*/
            DWORD dwRes = WaitForSingleObject(oEvent.hEvent,STATUS_CHECK_TIMEOUT);
            switch (dwRes)
            {
                // Event occurred.
            case WAIT_OBJECT_0:

                
                if (!GetOverlappedResult(port, &oEvent, &dwOvRes, FALSE))
                {
                    // An error occurred in the overlappedoperation;
                    // call GetLastError to find out what itwas
                    // and abort if it is fatal.
                    print_error("overlapped result fault");
                }
                else
                {
                    // Status event is stored in the eventflag
                    // specified in the originalWaitCommEvent call.
                    // leave event function if port is down
                    if (!comm_status(dwEvtMask))
                    {
                        printf("returned from eventcall");
                        return;
                    }
                }
                // WaitCommEvent is to be issued.
                fWaitingOnStat = FALSE;
                break;
            case WAIT_TIMEOUT:
                // Operation isn't complete yet. fWaitingOnStatusHandle flag
                // isn't changed since I'll loop back around and I don't want
                // to issue another WaitCommEvent until the first one finishes.
                break;
            default:
                // Error in the WaitForSingleObject; abort
                // This indicates a problem with the
                CloseHandle(oEvent.hEvent);
                print_error("Error in the WaitForSingleObject");
                break;
            }
        }

    }
    
}
