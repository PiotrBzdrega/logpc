#include "SerialCom.h"



SerialCom::SerialCom(const char* device_name, uint32_t com_speed) : device{ device_name }, baud_rate{ com_speed }, port{ nullptr }, old_dwModemStatus{}
{
    /* start port handling thread */
    handle_t = std::thread(&SerialCom::connection_loop, this);

    /* apply color for logs*/
    spdlog::stdout_color_mt("SerialCom");

#ifdef _DEBUG
    spdlog::set_level(spdlog::level::debug);
#else
    spdlog::set_level(spdlog::level::error);
#endif

    spdlog::debug("SerialCom instance created");


    CM_NOTIFY_FILTER NotifyFilter{ 0 };
    NotifyFilter.cbSize = sizeof(NotifyFilter);
    NotifyFilter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
    NotifyFilter.u.DeviceInterface.ClassGuid = GUID_DEVINTERFACE_COMPORT;




    //try to figure out how to read available interfaces then QT++
    CONFIGRET cr = CR_SUCCESS;
    PWSTR DeviceInterfaceList = NULL;
    ULONG DeviceInterfaceListLength = 0;
    PWSTR CurrentInterface;
    WCHAR CurrentDevice[MAX_DEVICE_ID_LEN];
    DEVINST Devinst = 0;
    WCHAR DeviceDesc[2048]{};
    DEVPROPTYPE PropertyType;
    ULONG PropertySize;
    DWORD Index = 0;

    do {
        cr = CM_Get_Device_Interface_List_Size(&DeviceInterfaceListLength,
            (LPGUID)&GUID_DEVINTERFACE_COMPORT,
            NULL,
            CM_GET_DEVICE_INTERFACE_LIST_ALL_DEVICES);

        if (cr != CR_SUCCESS)
        {
            break;
        }

        if (DeviceInterfaceList != NULL) {
            HeapFree(GetProcessHeap(),
                0,
                DeviceInterfaceList);
        }

        DeviceInterfaceList = (PWSTR)HeapAlloc(GetProcessHeap(),
            HEAP_ZERO_MEMORY,
            DeviceInterfaceListLength * sizeof(WCHAR));

        if (DeviceInterfaceList == NULL)
        {
            cr = CR_OUT_OF_MEMORY;
            break;
        }

        cr = CM_Get_Device_Interface_List((LPGUID)&GUID_DEVINTERFACE_COMPORT,
            NULL,
            DeviceInterfaceList,
            DeviceInterfaceListLength,
            CM_GET_DEVICE_INTERFACE_LIST_ALL_DEVICES);
    } while (cr == CR_BUFFER_SMALL);

    if (cr != CR_SUCCESS)
    {
        goto Exit;
    }

    for (CurrentInterface = DeviceInterfaceList;
        *CurrentInterface;
        CurrentInterface += wcslen(CurrentInterface) + 1)
    {

        _tprintf(_T("Serial Port Interface: %s\n"), CurrentInterface);

        PropertySize = sizeof(CurrentDevice);
        cr = CM_Get_Device_Interface_Property(CurrentInterface,
            &DEVPKEY_Device_InstanceId,
            &PropertyType,
            (PBYTE)CurrentDevice,
            &PropertySize,
            0);

        if (cr != CR_SUCCESS)
        {
            goto Exit;
        }

        if (PropertyType != DEVPROP_TYPE_STRING)
        {
            goto Exit;
        }
        wprintf(L"Property Value: %s\n", CurrentDevice);


        cr = CM_Get_DevNode_PropertyW(Devinst,
            &DEVPKEY_Device_FriendlyName,
            &PropertyType,
            (PBYTE)DeviceDesc,
            &PropertySize,
            0);


        wprintf(L"PropertyW Value: %s\n", DeviceDesc);

        // Since the list of interfaces includes all interfaces, enabled or not, the
        // device that exposed that interface may currently be non-present, so
        // CM_LOCATE_DEVNODE_PHANTOM should be used.
        cr = CM_Locate_DevNode(&Devinst,
            CurrentDevice,
            CM_LOCATE_DEVNODE_PHANTOM);

        if (cr != CR_SUCCESS)
        {
            goto Exit;
        }

        // Query a property on the device.  For example, the device description.
        PropertySize = sizeof(DeviceDesc);
        cr = CM_Get_DevNode_Property(Devinst,
            &DEVPKEY_Device_DeviceDesc,
            &PropertyType,
            (PBYTE)DeviceDesc,
            &PropertySize,
            0);

        if (cr != CR_SUCCESS)
        {
            goto Exit;
        }

        if (PropertyType != DEVPROP_TYPE_STRING)
        {
            goto Exit;
        }

        Index++;
    }

Exit:

    if (DeviceInterfaceList != NULL)
    {
        HeapFree(GetProcessHeap(),
            0,
            DeviceInterfaceList);
    }
    return;

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
            spdlog::error("Fault when Closing main Handle, {}", last_error());
        handle_closed = true;
    }
    else
        spdlog::debug("handle already closed");
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

std::string SerialCom::last_error()
{
    DWORD error_code = GetLastError();
    char buffer[256]{};
    DWORD size = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_MAX_WIDTH_MASK,
        NULL, error_code, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
        buffer, sizeof(buffer), NULL);
    return std::string(buffer);  
}


BOOL SerialCom::withdraw_buffer(SSIZE_T len)
{
    spdlog::debug("received telegram:{}  with size:{1}", (char*)read_buffer,len);

    /* allocate memory for new telegram +1 for null terminator*/
    uint8_t* credential = (uint8_t*)malloc(len+1);

    /* copy all telegram from buffer*/
    for (SSIZE_T i = 0; i < len; i++)
        credential[i] = read_buffer[i];
        
    /* null terminator in case of printf*/
    //credential[len] = '\0';

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
        spdlog::error("Failed to get serial settings, {}", last_error());
        //close_handle();
    }
    else
    {
        if (!SetCommState(port, &com_state))
        {
            spdlog::error("Failed to set serial settings, {}", last_error());
        }
        
        //printf("correctly get comm");
    }

    {
        std::lock_guard<decltype(mtx)> lk(mtx);

        if (!GetCommModemStatus(port, &dwModemStatus))
        {
            // Error in GetCommModemStatus;
            spdlog::error("GetCommModemStatus() error, {}", last_error());
            
            return false;
        }       
        if (old_dwModemStatus != dwModemStatus)
        {
            spdlog::debug("new modem status {:08x}", dwModemStatus);
            newModemStatus = true;
        }
        old_dwModemStatus = dwModemStatus;
        if (!(dwModemStatus & (MS_CTS_ON | MS_DSR_ON | MS_RLSD_ON | MS_RING_ON)))
        {
            //print_error("mutex locked\n");
            if (!port_down)
            {
                spdlog::debug("{:04x} port down", dwModemStatus);
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
                spdlog::debug("port up");
                port_down = false;
                cv.notify_all();
            }
        }
    }

    if ((state & EV_CTS) || newModemStatus)
    {
        spdlog::debug("CTS changed state value {}", bool(dwModemStatus & MS_CTS_ON));
    }

    if ((state & EV_DSR) || newModemStatus)
    {
        spdlog::debug("DSR changed state value {}", bool(dwModemStatus & MS_DSR_ON));
    }

    if ((state & EV_RLSD) || newModemStatus)
    {
        /*RLSD (Receive Line Signal Detect) is commonly referred to
         as the CD (Carrier Detect) line*/
        spdlog::debug("RLSD changed state value {}", bool(dwModemStatus & MS_RLSD_ON));
    }

    if ((state & EV_RING) || newModemStatus)
    {
        spdlog::debug("Ring signal detected value {}", bool(dwModemStatus & MS_RING_ON));
    }


    if (state & EV_RXCHAR)
    {
        spdlog::debug("Any Character received");
        SSIZE_T received= read_port();
        if (received)
        {
            withdraw_buffer(received);
        }
    }

    if (state & EV_RXFLAG)
    {
        spdlog::debug("Received certain character");
    }

    if (state & EV_TXEMPTY)
    {
        //TODO: why there is no information about sended telegram???
        spdlog::debug("Transmitt Queue Empty");
    }

    if (state & EV_BREAK)
    {
        spdlog::debug("BREAK received");
    }

    if (state & EV_ERR)
    {
        spdlog::debug("Line status error occurred");
    }

    if (state & EV_PERR)
    {
        spdlog::debug("Printer error occured");
    }

    if (state & EV_RX80FULL)
    {
        spdlog::debug("Receive buffer is 80 percent full");
    }

    if (state & EV_EVENT1)
    {
        spdlog::debug("Provider specific event 1");
    }

    if (state & EV_EVENT2)
    {
        spdlog::debug("Provider specific event 2");
    }

    spdlog::debug("comm_status");

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
        spdlog::error("{} , {}", device, last_error());
        return -1;
    }
    else
    {
        /* handle is up and running*/
        handle_closed = false;


        if (event_t.joinable())
        {
            spdlog::debug("event callback wait for join");
            event_t.join();       
        }
           

        /* start event thread */
        event_t = std::thread(&SerialCom::event_callback, this);
    }
    spdlog::debug("port opened succesfully");


    //if (DeviceIoControl(
    //    port,                        //HANDLE DEVICE
    //    IOCTL_SERIAL_GET_WAIT_MASK,  //dwIoControlCode
    //    NULL,                        //nInBufferSize
    //    0,                           //lpOutBuffer
    //    &dwWaitMask,                 //lpOutBuffer
    //    sizeof(DWORD),               //nOutBufferSize
    //    NULL,                        //lpBytesReturned
    //    NULL                         //lpOverlapped
    //)) {
        // Use the retrieved wait mask to determine which events are being waited for

    // Flush away any bytes previously read or written.
    BOOL success = FlushFileBuffers(port);
    if (!success)
    {
        spdlog::error("Failed to flush serial port , {}", device, last_error());
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
        spdlog::error("Failed to set serial timeouts , {}",last_error());
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
        spdlog::error("Failed to set serial settings , {}", last_error());
        close_handle();
        return -1;
    }

    success = GetCommState(port, &state);
    if (!success)
    {
        spdlog::error("Failed to get serial settings , {}", last_error());
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
            spdlog::debug("write_port| mutex released");
        else
        {
            spdlog::debug("write_port| mutex blocked");
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
        spdlog::error("Create event failed , {}", last_error());
        return;
    }

    BOOL success = WriteFile(port, buffer, size, NULL, &oWrite);
    if (!success)
    {   
        if (GetLastError() != ERROR_IO_PENDING)
        {
            spdlog::error("Failed to write to port , {}", last_error());
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
                            spdlog::error("Failed to write all bytes to port written:{}, size:{} , {}", written, size, last_error());
                            res = false;
                        }               
                        else
                        {
                            spdlog::debug("succesfully written:{}, size:{}", written, size);
                        }
                    }
                break;
            default:
                spdlog::error("Problem with OVERLAPPED event handle , {}", last_error());
                res = false;
                break;
            }
        }        
    }

    if (!CloseHandle(oWrite.hEvent))
        spdlog::error("Fault when Closing Handle oEvent.hEvent , {}", last_error());

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
        spdlog::error(" Create event failed, {}", last_error());
        return -1;
    }

    if (!waiting_on_read)
    {
        BOOL success = ReadFile(port, read_buffer, BUFFER_LEN, &received, &oRead);
        if (!success)
        {
            if (GetLastError() != ERROR_IO_PENDING)
            {
                spdlog::error("Failed to write to port , {}", last_error());
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
            spdlog::debug("read immediatelly");
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
                    spdlog::error("Failed to read from port, {} ", last_error());
                    return -1;
                }
                else
                {
                    /* read immediatelly done*/
                    spdlog::debug("read after waits");

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
        spdlog::error("Fault when Closing Handle oRead.hEvent , {}", last_error());

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
        spdlog::error("Failed to set communication mask , {}", last_error());
        return;
    }

    dwEvtMask = 0;

    success = GetCommMask(port, &dwEvtMask);
    if (!success)
    {
        spdlog::error("Failed to read communication mask, {}", last_error());
        return;
    }
    spdlog::debug("GetCommMask result {:04x}", dwEvtMask);

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
        spdlog::error("Create event failed, {}", last_error());
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
                    spdlog::debug("returned from eventcall");
                    return;
                }
                    
            }
            else
            {
                if (GetLastError() != ERROR_IO_PENDING)
                {
                    spdlog::error("communication error, {}", last_error());
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
                    spdlog::error("overlapped result fault, {}", last_error());
                }
                else
                {
                    // Status event is stored in the eventflag
                    // specified in the originalWaitCommEvent call.
                    // leave event function if port is down
                    if (!comm_status(dwEvtMask))
                    {
                        spdlog::debug("returned from eventcall");
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

                spdlog::error("Error in the WaitForSingleObject, {}", last_error());

                break;
            }
        }

    }
    
}
