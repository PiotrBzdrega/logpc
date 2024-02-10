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
    ///////////////////////////////////////////////////////////////////////////////////////////////

     // Step 1: Initialize variables and data structures

    /* The class identifier (CLSID) is a class that can display and/or 
    provide programmatic access to the property values.*/
    /* This class includes all Bluetooth devices. */
    const wchar_t* blthClsid = L"{e0cbf06c-cd8b-4647-bb8a-263b43f0f974}";

    HDEVINFO deviceInfoSet;
    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    DWORD index = 0;
    GUID deviceClassGuid; // This should be the GUID for the device class you are interested in.

    // Replace 'your_device_class_guid_here' with the actual GUID for your device class.
    // For example, for USB devices, you can use GUID_DEVINTERFACE_USB_DEVICE.
    if (CLSIDFromString(blthClsid, &deviceClassGuid) != S_OK) {
        std::cerr << "Error getting device class GUID." << std::endl;
        return;
    }

    // Step 2: Create a device information set for the specified device class
    deviceInfoSet = SetupDiGetClassDevs(&deviceClassGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (deviceInfoSet == INVALID_HANDLE_VALUE) {
        std::cerr << "Error creating device information set." << std::endl;
        return;
    }

    // Step 3: Enumerate device interfaces
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
    while (SetupDiEnumDeviceInterfaces(deviceInfoSet, NULL, (LPGUID)&GUID_DEVINTERFACE_COMPORT, index, &deviceInterfaceData)) {


        // Step 4: Retrieve information about the device interface
        // You can use this information to communicate with the device.
        // For example, you can get the device path and open a handle to it.

        // Step 5: Increment the index to enumerate the next device interface
        index++;
    }

    // Step 6: Cleanup
    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    spdlog::debug("indexes:{}",index);
    ////////////////////////////////////////////////////////////////////////////////////////////
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
            (LPGUID)&GUID_BTHPORT_DEVICE_INTERFACE,
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

        cr = CM_Get_Device_Interface_List((LPGUID)&GUID_BTHPORT_DEVICE_INTERFACE,
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
            &DEVPKEY_Device_ClassGuid,
            &PropertyType,
            (PBYTE)CurrentDevice,
            &PropertySize,
            0);

        //if (cr != CR_SUCCESS)
        //{
        //    goto Exit;
        //}

        //if (PropertyType != DEVPROP_TYPE_STRING)
        //{
        //    goto Exit;
        //}
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
    //return;


   
    auto propKey{ DEVPKEY_Device_PDOName };
    ULONG len{};
    auto res = CM_Get_Device_ID_List_Size(&len, blthClsid, CM_GETIDLIST_FILTER_CLASS | CM_GETIDLIST_FILTER_PRESENT);
    if (res != CR_SUCCESS) {
        std::cerr << "error num " << res << " occured\n";
        return ;
    }

    PWSTR devIds = (PWSTR)HeapAlloc(GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        len * sizeof(WCHAR));

    if (devIds == NULL)
    {
        cr = CR_OUT_OF_MEMORY;
        std::cout << "fault\n";
        return;
    }

    res = CM_Get_Device_ID_ListW(blthClsid, devIds, len, CM_GETIDLIST_FILTER_CLASS | CM_GETIDLIST_FILTER_PRESENT);
    std::wcout << L"installed serial devices:\n";

    for (CurrentInterface = devIds;
        *CurrentInterface;
        CurrentInterface += wcslen(CurrentInterface) + 1)
    {

        _tprintf(_T("Serial Port Interface: %s\n"), CurrentInterface);
        DEVINST devInst{};

        DEVPROPTYPE devpropt{};
        CM_Get_DevNode_PropertyW(devInst, &propKey, &devpropt, nullptr, &len, 0);

        if (res != CR_BUFFER_SMALL && res != CR_SUCCESS) {
            std::cerr << "error " << res << "\n";
            continue;
            //return -1;
        }
        auto buffer = std::make_unique<BYTE[]>(len);
        res = CM_Get_DevNode_PropertyW(devInst, &propKey, &devpropt, buffer.get(), &len, 0);
        if (devpropt == DEVPROP_TYPE_STRING) {
            const auto val = reinterpret_cast<wchar_t*>(buffer.get());
            std::wcout << L"friendly name: " << val << L"\n\n";
        }
    }

   


    

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
    spdlog::debug("received telegram:{0} with size:{1}", (char*)read_buffer,len);

    /* allocate memory for new telegram +1 for null terminator*/
    /*uint8_t* credential = (uint8_t*)malloc(len+1);*/

    /* copy all telegram from buffer*/
    //for (SSIZE_T i = 0; i < len; i++)
    //    credential[i] = read_buffer[i];
        
    /* null terminator in case of printf*/
    //credential[len] = '\0';

    /* stop UIHandle callback thread if pending */
    if (callback_t.joinable())
        callback_t.join();

    /* start thread for UIHandle thread*/
    callback_t = std::thread(callback, std::string(reinterpret_cast<char*>(read_buffer), len));

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
            spdlog::debug("new modem status 0x{:04x}", dwModemStatus);
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
        SSIZE_T received = read_port();
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
        //TODO:what is it
        /* till port is down*/
        std::unique_lock<decltype(mtx)> lk(mtx);

        //TODO:what is it
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


    /* OVERLAPPED structure is used for asynchronous I / O operations,
       allowing you to perform I / O operations in a non - blocking manner. */
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

            /* If the object's state is nonsignaled, 
            the calling thread enters the wait state until the object
            is signaled or the time-out interval elapses. */
            /* If dwMilliseconds is INFINITE, 
            the function will return only when the object is signaled.*/
            //TODO: why we wait infinite amount of time for write?
            DWORD dwRes = WaitForSingleObject(oWrite.hEvent,INFINITE);

            switch (dwRes)
            {          
                case WAIT_OBJECT_0: /*  OVERLAPPED event Event has been signaled.*/
                    success = GetOverlappedResult(port, &oWrite, &written, bwait);
                    if (!success)
                        res = false;
                    else
                    {
                        res = true;
                        // check if all data has been sent
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
                spdlog::error("WaitForSingleObject() function has failed with Write HANDLE, {} ", last_error());
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

    /* OVERLAPPED structure is used for asynchronous I / O operations, 
       allowing you to perform I / O operations in a non - blocking manner. */   
    /* handle for read operation */
    OVERLAPPED oRead = { 0 };
    bool bwait = false;

    /* used to create a synchronization event object. 
       Event objects are synchronization primitives used for inter-process or 
       inter-thread communication to notify when an event has occurred. */
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

        /* If the object's state is nonsignaled,
           the calling thread enters the wait state until the object
           is signaled or the time-out interval elapses. */
        dwRes = WaitForSingleObject(oRead.hEvent, READ_TIMEOUT);
        BOOL success;

        switch (dwRes)
        {
            case WAIT_OBJECT_0: /* The state of the specified object is signaled. */

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
                break;

                //TODO: seems that this must be inside while loop till return from WaitForSingleObject will be not time-out
            case WAIT_TIMEOUT: /* The time-out interval elapsed, and the object's state is nonsignaled. */
                // Operation isn't complete yet. fWaitingOnRead flag isn't
                // changed since I'll loop back around, and I don't want
                // to issue another read until the first one finishes.
                spdlog::error("ReadFile not finished succesfully => WAIT_TIMEOUT, it must be fixed (loop) TODO!!!!!!!!!!!!!!!!!! ");               
                break;

            default:
                // Error in the WaitForSingleObject probably WAIT_FAILED; abort.
                // This indicates a problem with the OVERLAPPED structure's
                // event handle.
                spdlog::error("WaitForSingleObject() function has failed with Read HANDLE, {} ", last_error());
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
    spdlog::debug("GetCommMask result 0x{:04x}", dwEvtMask);

    /* OVERLAPPED structure is used for asynchronous I / O operations,
       allowing you to perform I / O operations in a non - blocking manner. */

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
            /* Waits for an event to occur for a specified communications device */
            if (WaitCommEvent(port, &dwEvtMask, &oEvent))
            {
                /* State recognition and port activity update*/
                if (!comm_status(dwEvtMask))
                {
                    // leave event function if port is down
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
                    /* State recognition and port activity update */
                    if (!comm_status(dwEvtMask))
                    {
                        // leave event function if port is down
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
