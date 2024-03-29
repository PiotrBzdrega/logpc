﻿#include "UIHandle.h"

UIHandle::UIHandle()
{
    /* apply color for logs*/
    spdlog::stdout_color_mt("UIHandle");


    std::ifstream jsonFileStream("./credentials.json");
    if (!jsonFileStream.is_open())
    {
        spdlog::error("missing .json file with credentials");
    }
    else
    {
        nlohmann::json jsonData = nlohmann::json::parse(jsonFileStream);

        // Process the JSON data
        std::vector<UICredential> credentialsList;
        for (const auto& entry : jsonData) {
            UICredential credentials;

            credentials.ui_domain = entry["domain"];

            credentials.ui_login = entry["login"];

            credentials.ui_password = entry["password"];
             
            credentialsList.push_back(credentials);
        }

        //TODO: 
        /* initialize domains credentials*/
        ui_init_dict(credentialsList);
    }
    
#ifdef _DEBUG
    spdlog::set_level(spdlog::level::debug);
#else
    spdlog::set_level(spdlog::level::error);
#endif



    /* CoInitializeEx() Initializes the COM library for use by the calling thread,
       sets the thread's concurrency model, and creates a new apartment for the thread 

       https://learn.microsoft.com/en-us/windows/win32/api/objbase/ne-objbase-coinit  says that
       for GUI I should use COINIT_APARTMENTTHREADED instead COINIT_MULTITHREADED*/ 

    /* CoCreateInstance() create an instance of a COM class identified by its CLSID (Class Identifier). 
    CLSIDs uniquely identify COM classes and are typically stored in the Windows registry. */
    if (CoInitializeEx(nullptr, COINIT_MULTITHREADED) == S_OK && SUCCEEDED(uia.CoCreateInstance(CLSID_CUIAutomation)))
    {
        spdlog::debug("Instance of UIAutomation successfully created");
    }
    else
    {
        spdlog::error("Failed to Create instance of UIAutomation");
    }

}

UIHandle::~UIHandle()
{
    /* stop wakeup thread if pending */
    if (wakeup_t.joinable())
        wakeup_t.join();

    /* stop SerialCom callback thread*/
    if (callback_t.joinable())
        callback_t.join();

    /* release smart pointer*/
    uia.Release();

    /* Closes the COM library on the current thread, unloads all DLLs loaded by the thread,
       frees any other resources that the thread maintains, and forces all RPC connections on the thread to close. */
    CoUninitialize();

    /* release domains credentials*/
    release_dict();
}

void UIHandle::prepare_notification(UI_ENUM mode,std::string domain)
{
    /* stop SerialCom callback thread if pending */
    if (callback_t.joinable())
        callback_t.join();

    /* create acknowledge message*/
    int len = create_message(mode, domain);

    /* start thread for SerialCom callback*/
    callback_t = std::thread(callback, telegram, len);
}

bool UIHandle::add_callback(SerialComCb callback)
{
    /* assign to class callback*/
    this->callback = callback; 
    return true;
}

int UIHandle::create_message(UI_ENUM mode, std::string domain)
{
    spdlog::debug("elements to create message: {} and {}", (int)mode, domain.c_str());

    /* pointer for telegram*/
    uint8_t* tmp = telegram;

    /* mode int to char and move pointer*/
    *tmp++ = static_cast<uint8_t>(mode + '0');

    /* domain string has at least one character*/
    if (domain.size())
    {
        /* insert delimiter and move pointer forward*/
        *tmp++ = ',';

        /* copy domain name into telegram*/
        for (size_t i = 0; i < domain.size(); i++)
            *tmp++ = domain[i];

        /* insert ending null character */
        *tmp = '\0';
    }

    spdlog::debug("created msg: {}", reinterpret_cast<char*>(telegram));

    /* return difference of pointers*/
    return (tmp - telegram);
}


void UIHandle::process_data(std::string authorization)
{
    //spdlog::debug("received telegram from SERIALCOM:{}   with size: {}", reinterpret_cast<char*>(buffer), size);

    //reinterpret uint8_t* to char* is followed by convert the char* to std::string
    //std::string input(reinterpret_cast<const char*>(buffer),size);

    //release pointer to credentials
    //free(buffer);

    spdlog::debug("received telegram from SERIALCOM:{} ", authorization);
    //spdlog::debug("created string input: {}", input);


    // Regular expression pattern to match words between commas and first digit.
    std::regex pattern("^(\\d)(?:,([^,\\s]+))?(?:,([^,\\s]+))?(?:,([^,\\s]+))?$");

    /* check if pattern covers input */
    if (std::regex_match(authorization, pattern))
    {
        std::stringstream ss(authorization);
        std::string word;

        std::vector <std::string> telegram_part;
        /* split words in match*/
        while (std::getline(ss, word, ',')) 
        {
            spdlog::debug("new word in regex found : {}", word);
            /* add new element to list */
            telegram_part.push_back(word);            
        }
        
        spdlog::debug("received correct telegram {0}, with {1} element{2}", authorization, telegram_part.size(), (telegram_part.size() > 1) ? "s" : "");

        spdlog::debug("regex recognized following parts:");

        for (const auto& i :telegram_part)
            spdlog::debug("element: {}",i);

        /* according regex pattern 0 element is a digit example: char '2' -> int 50. 50-48('0')=2*/
        UI_ENUM mode = static_cast<UI_ENUM>(telegram_part[0][0]-'0');

        /* Start initialization if only if specific mode has been received*/
        if (mode == UI_DOMAIN   or
            mode == UI_LOGIN    or 
            mode == UI_PASSWORD or 
            mode == UI_LOGPASS)
        {
            /* start initialization*/
            if (!initialize_instance())
            {
                /* initialization failed*/
                prepare_notification(UI_FAIL, "");
                
                spdlog::error("incomming message dropped without processing");
                return;
            }
        }
        spdlog::debug("initialization done");

        switch (mode)
        {
        case UI_DOMAIN :
            spdlog::debug("telegram requests UI_DOMAIN");
            /* stop wakeup thread if pending */
            if (wakeup_t.joinable())
                wakeup_t.join();

            /* start thread for wakeup functionality*/
            wakeup_t = std::thread(&UIHandle::look_for_web_field,this);
            break;
        case UI_LOGIN:
        case UI_PASSWORD:

            spdlog::debug("telegram requests {}", (mode == UI_LOGIN) ? "UI_LOGIN" : "UI_PASSWORD");
            /* following telegram should contain 3 elements in telegram*/
            if (telegram_part.size() == 3)
            {
                /* enter found credential*/
                if (credential(mode, telegram_part[1].c_str(), telegram_part[2].c_str()))
                {
                    /* press enter*/
                    accept_credenetial();

                    prepare_notification(UI_DONE, telegram_part[1]);                 
                }
                else
                {
                    spdlog::error("failed to enter {} credential", (mode == UI_LOGIN) ? "UI_LOGIN" : "UI_PASSWORD");

                    prepare_notification(UI_FAIL, telegram_part[1]);
                }
            }
            else
            {
                spdlog::error("wrong amount of elements regex found :{} ", telegram_part.size());
            }
            break;
        case UI_LOGPASS:
            spdlog::debug("telegram requests UI_LOGPASS");
            if (telegram_part.size() == 4)
            {
                /* enter found login*/
                if (credential(UI_LOGIN, telegram_part[1].c_str(), telegram_part[2].c_str()))
                {   
                    /* enter found password*/
                    if (credential(UI_PASSWORD, telegram_part[1].c_str(), telegram_part[3].c_str()))
                    {
                        /* press enter*/
                        accept_credenetial();

                        prepare_notification(UI_DONE, telegram_part[1]);
                    }
                    else
                    {
                        spdlog::error("failed to enter UI_PASSWORD credential");

                        prepare_notification(UI_FAIL, telegram_part[1]);
                    }

                }
                else
                {
                    spdlog::error("failed to enter UI_LOGIN credential");

                    prepare_notification(UI_FAIL, telegram_part[1]);
                }
            }
            else
            {
                spdlog::error("wrong amount of elements regex found :{} ", telegram_part.size());
            }
                

            break;
        case UI_DONE:
            break;
        case UI_NEW_CREDENTIAL:
            break;
        case UI_ERASE:
            break;
        case UI_MISSED:
            spdlog::debug("telegram requests UI_MISSED");
            break;
        default:
            spdlog::error("unknown mode received {}", (int)mode);
            break;
        }
    }
    else
    {
        spdlog::error("unknown message template has been received {}", authorization);
    }

    return;
}

void UIHandle::look_for_web_field()
{
    /* time stamp now*/
    auto startTime = std::chrono::high_resolution_clock::now();

    /* result of function invocation*/
    ret_url res = { nullptr,1 };
    while (true)
    {
        // Calculate the elapsed time
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();

        // Check if the loop duration is over (5 seconds)
        if (elapsedTime >= 5)
        {
            spdlog::error(" searching url timeout");
            return;
        }

        /* search for known website*/
        res = find_url("");

        /* fatal error*/
        if (!res.val)
            return;
        else if (res.val and res.domain)
        {
            /* found familiar domain*/

            /* check what elements are available on site*/
            UI_ENUM element = which_element(res.domain);


            if (element not_eq UI_ENUM::UI_UNKNOWN)
            {;
                /* request for credentials*/
                prepare_notification(element, res.domain);
                return;
            }
            
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

}

bool UIHandle::initialize_instance() 
{
    HWND hwnd;

    auto startTime = std::chrono::high_resolution_clock::now();

    while (true)
    {

        // Calculate the elapsed time
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
        //printf("elapsedTime :%d\n",elapsedTime);
        /* through 10 seconds, initialization cannot be finished*/
        if (elapsedTime >= 10)
        {
            spdlog::error("initialization timeout");
            return false;
        }

        /* handle is not nullptr*/
        if (window_handle)
        {
            /* Window has title and is not closed/minimized */
            if (IsWindowVisible(window_handle) && !IsIconic(window_handle) && GetWindowTextLength(window_handle) > 0)
            {
                spdlog::debug("elapsedTime: {}seconds", elapsedTime);
                int c = GetWindowTextLength(window_handle);
                //printf("%d\n", c);
                LPWSTR pszMem = (LPWSTR)malloc(sizeof(LPWSTR) * (c + 1));
                
                GetWindowText(window_handle, pszMem, c + 1);

                /* null terminator; GetWindowText doesn't attach it*/
                pszMem[c] = L'\0';
                //TODO: implement spdlog for LPWSTR
                wprintf(L"Window Text: %s", pszMem);

                free(pszMem);

                break;
            }
        }

        HWND child_handl = nullptr; // always nullptr

        /* Windows API function used to retrieve a handle to a child window */
        /* check if some window has className "Chrome_WidgetWin_1"*/
        window_handle = FindWindowEx(nullptr, child_handl, L"Chrome_WidgetWin_1", nullptr);
        if (!window_handle)
        {
            spdlog::error("handle from FindWindowEx is nullptr");
            return false;
        }
        
    }

    spdlog::debug("root pointer :{}, uia pointer :{}", static_cast<void*>(root.p), static_cast<void*>(uia.p));

        root = nullptr;

    if SUCCEEDED(uia->ElementFromHandle(window_handle, &root))
    {
        return true;
    }

   return false;
}

bool UIHandle::accept_credenetial()
{

    INPUT input[2] = { INPUT_KEYBOARD };
    input[0].ki.wVk = VK_RETURN;
    input[1] = input[0];
    input[1].ki.dwFlags |= KEYEVENTF_KEYUP;
    UINT uSent = SendInput(ARRAYSIZE(input), input, sizeof(INPUT));

    return 0;
}

char* UIHandle::domain_recognition(const char* url,const char* req_dom)
{
    char* domain=nullptr;
    int first_let = 0;
    int last_let = 0;
    /*"com","net","pl","org","gov","de"
    /*"stackoverflow","yandex","gmail","youtube","github","linkedin"
    domain search*/

    bool first_found = false;
    bool hit = false;
    for (int i = 0; (url[i] != '/' || !first_found) && url[i] != '\0' && !hit; i++)
    {

        //found dot on the way
        if (url[i] == '.' || url[i] == '/')
        {
            if (url[i] == '/' && not first_found)
            {
                //https://
                i++;
                last_let = i;
                continue;
            }

            if (last_let != first_let)
                first_let = last_let + 1;

            last_let = i;
            char* page = _strdup(&url[first_let]);
            page[last_let - first_let] = '\0';
            spdlog::debug("{}", page);

            if (ui_lookup(page))
            {
                /* requested domain page has some characters*/
                if (req_dom!="")
                {
                    /* compare requested domain with recognized alias of page*/
                    if (strcmp(page, req_dom)==0)
                    {
                        hit = true;
                        domain = page;
                    };
                }
                else
                {
                    hit = true;
                    domain = page;
                }               
                spdlog::debug("hit hashtable");
            }
            else
            {
                /* release memory if we don't have such domain in dictionary*/
                free(page);
                page = nullptr;

                spdlog::debug("miss hashtable");
            }

            //printf("%s\n", strcmp(page, "yandex")==0 ? "success" : "miss");
            first_found = true;
        }
    }
    return domain;
}

ret_url UIHandle::find_url(const char* req_dom)
{
    constexpr ret_url ERR{ nullptr, false };

    //result value (default value, not found, but correctly finished)
    ret_url res{ nullptr,true };
    // The root window has several childs, 
    // one of them is a "pane" named "Google Chrome"
    // This contains the toolbar. Find this "Google Chrome" pane:
    CComPtr<IUIAutomationElement> pane;
    CComPtr<IUIAutomationCondition> pane_cond;
    uia->CreatePropertyCondition(UIA_ControlTypePropertyId,
        CComVariant(UIA_PaneControlTypeId), &pane_cond);

    CComPtr<IUIAutomationElementArray> arr;
    if FAILED(root->FindAll(TreeScope_Children, pane_cond, &arr))
    {

        spdlog::error("root cannot find correct element");
        return (res=ERR);
    }
    int count = 0;
    arr->get_Length(&count);
    spdlog::debug("editable_length:{}", count);

    for (int i = 0; i < count; i++)
    {
        CComBSTR name;

        if SUCCEEDED(arr->GetElement(i, &pane))
        {
            pane->get_CurrentClassName(&name);
            wprintf(L"%ls\n", (wchar_t*)name);
            uint8_t length = SysStringLen(name);
            SysFreeString(name);
            if (!length) //not correct element had length 23 "Intermediate D3D Window"; get_CurrentName gives nothing :(C
                break;
        }
        pane.Release();
    }

    if (!pane)
    {
        spdlog::error("pointer error: pane is nullptr");
        return (res = ERR);
    }
        
    //look for first UIA_EditControlTypeId under "Google Chrome" pane
    CComPtr<IUIAutomationElement> url;
    CComPtr<IUIAutomationCondition> url_cond;
    uia->CreatePropertyCondition(UIA_ControlTypePropertyId,
        CComVariant(UIA_EditControlTypeId), &url_cond);
    if FAILED(pane->FindFirst(TreeScope_Descendants, url_cond, &url))
    {
        spdlog::error("pane cannot find correct element");
        return (res = ERR);
    }
        

    if (!url)
    {
        spdlog::error("pointer error: url is nullptr");
        return (res = ERR);
    }

    //CComPtr<IUIAutomationElementArray> url;
    //if FAILED(pane->FindAll(TreeScope_Descendants, url_cond, &url))
    //    return false;

    //count = 0;
    //url->get_Length(&count);
    //printf("editable_length:%d\n", count);

    //for (int i = 0; i < count; i++)
    //{
    //    CComBSTR name;

    //    if SUCCEEDED(url->GetElement(i, &pane))
    //    {

    //        pane->get_CurrentClassName(&name);
    //        wprintf(L"%ls\n", (wchar_t*)name);
    //        uint8_t length = SysStringLen(name);
    //        SysFreeString(name);
    //        if (!length) //not correct element had length 23 "Intermediate D3D Window"; get_CurrentName gives nothing :(C
    //            break;
    //    }
    //    pane.Release();

    //}



    //get value of `url`
    CComVariant var;


    //if FAILED(pane->GetCurrentPropertyValue(UIA_ValueValuePropertyId, &var))
    //    return false;
     //TODO: here url is null pointer when brovser is not in front or minimized (but only on first shot 😐) 
    if FAILED(url->GetCurrentPropertyValue(UIA_ValueValuePropertyId, &var))
    {
        spdlog::error("url cannot GetCurrentPropertyValue ");
        return (res = ERR);
    }
    if (!var.bstrVal)
    {
        spdlog::error("Variant string has nullpointer");
        return (res = ERR);
    }
    wprintf(L"find_url: %s\n", var.bstrVal);

    int length = SysStringLen(var.bstrVal);
    spdlog::debug("{}", length);
    size_t t;
    char* mbstring = (char*)malloc(length + 1);

    wcstombs_s(&t, mbstring, length + 1, var.bstrVal, length);
    spdlog::debug("converted string: {} size is: {}", mbstring, t);

    res.domain = domain_recognition(mbstring,req_dom);

    free(mbstring);
    //set new address ...
    IValueProvider* pattern = nullptr;
    //if (FAILED(pane->GetCurrentPattern(UIA_ValuePatternId, (IUnknown**)&pattern)))
    //    return false;
    if (FAILED(url->GetCurrentPattern(UIA_ValuePatternId, (IUnknown**)&pattern)))
    {
        spdlog::error("url cannot GetCurrentPattern ");
        return (res = ERR);
    }
    //pattern->SetValue(L"google.com");
    pattern->Release();

    return res;
}

CComPtr<IUIAutomationElement> UIHandle::find_element(UI_ENUM ui, const char* domain)
{
    // The root window has several childs, 
    // one of them is a "pane" named "Google Chrome"
    // This contains the toolbar. Find this "Google Chrome" element:

    
    CComPtr<IUIAutomationElement> element;
    CComPtr<IUIAutomationCondition> pane_cond;
    CComVariant var;
    HRESULT res;
    std::shared_ptr<ui_nlist> map_entry = ui_lookup(domain);
    if (!map_entry)
    {
        spdlog::error("shared_ptr<ui_nlist> allocation error: map_entry is nullptr");
        return nullptr;
    }

    /* default property for searching datafield*/
    PROPERTYID property = UIA_AutomationIdPropertyId;

    /* request for login field*/
    if (ui == UI_ENUM::UI_LOGIN)
    {
        var.bstrVal = SysAllocString(CA2W(map_entry->defn.ui_login.c_str()));
        wprintf(L"allocated String: %ls\n", var.bstrVal);
        var.vt = VT_BSTR;

        if (!var.bstrVal)
        {
            spdlog::error("string allocation error: var.bstrVal is nullptr");
            return nullptr;
        }
    }
    /* request for password field*/
    else if (ui == UI_ENUM::UI_PASSWORD)
    {
        spdlog::debug("password property");
        var.boolVal = VARIANT_TRUE;
        var.vt = VT_BOOL;
        property = UIA_IsPasswordPropertyId;
    }
    /* wrong request*/
    else
    {
        spdlog::error("Wrong UI_ENUM ");
        return nullptr;
    }

    res = uia->CreatePropertyCondition(property, var, &pane_cond);
    if FAILED(res)
    {
        spdlog::error("#### condition property creation error: CreatePropertyCondition() returns %08x \n", res);
        pane_cond.Release();
        return nullptr;
    }

    if (ui == UI_ENUM::UI_LOGIN)
    {   /*release memory if has been used*/
        SysFreeString(var.bstrVal);
    }

    res = root->FindFirst(TreeScope_Descendants, pane_cond, &element);
    if FAILED(res)
    {
        spdlog::error("#### finding error: FindFirst() returns %08x \n", res);
        pane_cond.Release();
        return nullptr;
    }

    if (!element)
    {
        spdlog::error("#### element allocation error: element is nullptr \n");
        return nullptr;
    }

    pane_cond.Release();
    CComBSTR name;
    element->get_CurrentAutomationId(&name);
    wprintf(L"name:%ls\n", (wchar_t*)name);
    SysFreeString(name);

    
    spdlog::debug("[{}] found element address:{}", std::string(__FUNCTION__), static_cast<void*>(element));
    return element;
}

UI_ENUM UIHandle::which_element(const char* domain)
{
    // The root window has several childs, 
    // one of them is a "pane" named "Google Chrome"
    // This contains the toolbar. Find this "Google Chrome" element:
    
    UI_ENUM ret_val= UI_ENUM::UI_UNKNOWN;
    std::shared_ptr<ui_nlist> map_entry = ui_lookup(domain);
    if (!map_entry)
    {
        spdlog::error("#### shared_ptr<ui_nlist> allocation error: map_entry is nullptr \n");
        return UI_ENUM::UI_UNKNOWN;
    }

    /* currently considered field UI_ENUM*/
    int ui;

    /* search for first available field Login or Password*/
    for ( ui= UI_ENUM::UI_LOGIN; ui <= UI_ENUM::UI_PASSWORD; ui++)
    {
        HRESULT res;
        CComVariant var;
        PROPERTYID property;

        if (ui == UI_ENUM::UI_LOGIN)
        {
            var.bstrVal = SysAllocString(CA2W(map_entry->defn.ui_login.c_str()));
            wprintf(L"allocated String: %ls\n", var.bstrVal);
            var.vt = VT_BSTR;
            property = UIA_AutomationIdPropertyId;

            if (!var.bstrVal)
            {
                spdlog::error("#### string allocation error: var.bstrVal is nullptr \n");
                continue;
            }
        }
        else if (ui == UI_ENUM::UI_PASSWORD)
        {
            spdlog::debug("password property\n");
            var.boolVal = VARIANT_TRUE;
            var.vt = VT_BOOL;
            property = UIA_IsPasswordPropertyId;
        }

        /* condition to find UI Element*/
        CComPtr<IUIAutomationCondition> pane_cond;


        //UIA_HasKeyboardFocusPropertyId

        res = uia->CreatePropertyCondition(property, var, &pane_cond);
        if FAILED(res)
        {          

            printf("#### condition property creation error: CreatePropertyCondition() returns %08x \n", res);
            pane_cond.Release();

            /*BSTR has been used for UI_LOGIN */
            if (ui == UI_ENUM::UI_LOGIN)
                SysFreeString(var.bstrVal);

            continue;
        }

        if (ui == UI_ENUM::UI_LOGIN)
        {   /*release memory if has been used*/
            SysFreeString(var.bstrVal);
        }

        /* Login/Password field*/
        CComPtr<IUIAutomationElement> element;

        res = root->FindFirst(TreeScope_Descendants, pane_cond, &element);
        if FAILED(res)
        {
                
            spdlog::error("#### finding error: FindFirst() returns %08x \n", res);
            pane_cond.Release();
            continue;
        }

        if (!element)
        {              
            spdlog::error("#### element allocation error: element is nullptr \n");
            continue;
        }

        pane_cond.Release();
        CComBSTR name;
        element->get_CurrentAutomationId(&name);
        wprintf(L"name:%ls\n", (wchar_t*)name);
        SysFreeString(name);
        spdlog::debug("{}", static_cast<void*>(element));
        
        //var.vt = VT_BSTR;
        //TODO: func says that fields are empty but seems to be differently 
        /* check if field is filled out or empty*/
        if FAILED(element->GetCurrentPropertyValue(UIA_ValueValuePropertyId, &var))
        {
            printf("#### current property value error: GetCurrentPropertyValue() returns %08x \n", res);
            continue;
        }
        if (!var.bstrVal)
        {
            printf("#### element allocation error: element is nullptr \n");
            continue;
        }

        wprintf(L"content of field: %s\n", var.bstrVal);

        /* field is available to insert credential */
        if (/* var.bstrVal[0] == L'\0' && */ ui == UI_ENUM::UI_LOGIN ||
            ui == UI_ENUM::UI_PASSWORD)
        {
            /* UI_LOGIN already stored in ret_val -> BOTH fields available */
            if (ret_val== UI_ENUM::UI_LOGIN)
                ret_val = UI_ENUM::UI_LOGPASS;
            /* save found UI*/
            else
                ret_val = static_cast<UI_ENUM>(ui); 
        }        
        SysFreeString(var.bstrVal);
        element.Release();
    }
    return ret_val;
}

bool UIHandle::credential(UI_ENUM ui_mode, const char* domain, const char* credential)
{
    ret_url url = find_url(domain);
    if (!url.domain)
    {
        spdlog::error("#### finding requested url error");
        return false;
    }

    CComPtr<IUIAutomationElement> elem = find_element(ui_mode, domain);

    if (!elem)
    {
        spdlog::error("#### element finding error: {} for [{}] domain has not been found \n", (ui_mode == 1) ? "login" : "password", domain);
        return false;
    }

    spdlog::debug("[{}] found element address:{}", std::string(__FUNCTION__), static_cast<void*>(elem));

    CComBSTR name;
    elem->get_CurrentAutomationId(&name);
    wprintf(L"name:%ls\n", (wchar_t*)name);
    SysFreeString(name);

    CComVariant var;

    if FAILED(elem->GetCurrentPropertyValue(UIA_ValueValuePropertyId, &var))
        return false;
    if (!var.bstrVal)
        return false;

    wprintf(L"content: %s\n", var.bstrVal);
    SysFreeString(var.bstrVal);

    int len = (int)strlen(credential);

    wchar_t* w_text = (wchar_t*)malloc(sizeof(wchar_t) * (len + 1));
    size_t t;

    errno_t res = (mbstowcs_s(&t, w_text, len + 1, credential, len));
    if (res)
    {
        spdlog::error("### conversion error: mbstowcs_s() returns {}  ", res);
        return false;
    }

    spdlog::debug("converted characters:{}", t);
    wprintf(L"%s\n", w_text);

    IValueProvider* pattern = nullptr;
    if (FAILED(elem->GetCurrentPattern(UIA_ValuePatternId, (IUnknown**)&pattern)))
        return false;

    if (!pattern)
    {
        spdlog::error("### pointer error: pattern is nullptr  \n");
        return false;
    }

    pattern->SetValue(w_text);
    //pattern->get_Value(&var.bstrVal);


    if FAILED(elem->GetCurrentPropertyValue(UIA_ValueValuePropertyId, &var))
    {
        spdlog::error("### GetCurrentPropertyValue error: cannot check inserted credential \n");
        return false;
    }
       
    if (!var.bstrVal)
    {
        spdlog::error("### string allocation memory error: !var.bstrVal is nullptr \n");
        return false;
    }

    wprintf(L"inserted content: %s\n", var.bstrVal);



    if (w_text != var.bstrVal)
    {
        spdlog::error("### set text is not same like get \n");
        //TODO: return fault if texts are not the same
        //return false;
    }
    
    SysFreeString(var.bstrVal);

    // Check if the element HasKeyboardFocus property
    VARIANT_BOOL hasKeyboardFocus;
    if FAILED(elem->GetCurrentPropertyValue(UIA_HasKeyboardFocusPropertyId, &var))
    {
        spdlog::error("### GetCurrentPropertyValue error: cannot check if element HasKeyboardFocus\n");
        //return false;
    }

    if (var.boolVal == VARIANT_TRUE) 
        spdlog::debug("The element has keyboard focus.");
    else 
        spdlog::debug("The element does not have keyboard focus.");
    
    pattern->Release();
    free(w_text);
    //SysFreeString(var.bstrVal);


    elem.Release();

    spdlog::debug("finished succesfully\n");
    return true;
}