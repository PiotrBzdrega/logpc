#include "UIHandle.h"

UIHandle::UIHandle()
{
    //TODO: attach it as part of object instance
    IUIAutomationMap ui_element[HASHSIZE] = { {"github","login_field","password"},
                                  {"etutor","login","haslo"},
                                  {"facebook","email","pass"},
                                  {"centrum24","input_nik","ordinarypin"},
                                  {"google", "identifierId", ""}, //"passwordId"},
                                  {"yandex","passp-field-login","passp-field-passwd"},
                                  {"linkedin","username","password"},
                                  {"soundcloud","sign_in_up_email","password"},
                                  {"wikipedia","wpName1","wpPassword"},
                                  {"facebook","email",""},
                                  {"quora","email","password"},
                                  {"onet","email","password"},
                                  {"wp","login","password"},
                                  {"stackoverflow","email","password"}

    };

    /* initialize domains credentials*/
    init_dict(ui_element);

    //TODO: comment
    HRESULT res = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if SUCCEEDED(uia.CoCreateInstance(CLSID_CUIAutomation))
    {
        ;
    }
    else
    {
        printf("Failed to Create instance of UIAutomation\n");
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

    //TODO: comment
    CoUninitialize();

    /* release domains credentials*/
    release_dict();
}

void UIHandle::prepare_notification(UI_ENUM mode,std::string domain)
{
    printf("before callback_t.joinable()\n");
    /* stop SerialCom callback thread if pending */
    if (callback_t.joinable())
        callback_t.join();

    printf("before create_message(mode, domain)\n");

    /* create acknowledge message*/
    int len = create_message(mode, domain);

    printf("before std::thread(callback, telegram, len);\n");

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
    printf("elements to create message: %d and %s\n",mode, domain.c_str());

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

    printf("created msg: %s\n", telegram);

    /* return difference of pointers*/
    return (tmp - telegram);
}


void UIHandle::process_data(uint8_t* buffer, size_t size)
{
   
    printf("received telegram from SERIALCOM:%s \t with size:%d\n", (char*)buffer, size);


    //reinterpret uint8_t* to char* is followed by convert the char* to std::string
    std::string input(reinterpret_cast<const char*>(buffer),size);

    std::cout << input<<'\n';

    // Regular expression pattern to match words between commas and first digit.
    std::regex pattern("^(\\d)(?:,([^,\\s]+))?(?:,([^,\\s]+))?(?:,([^,\\s]+))?$");

    /* check if pattern covers input */
    if (std::regex_match(input, pattern))
    {
        std::stringstream ss(input);
        std::string word;

        std::vector <std::string> telegram_part;
        /* split words in match*/
        while (std::getline(ss, word, ',')) 
        {
            std::cout << word<<'\n';
            /*printf("%s\n", word);*/
            /* add new element to list */
            telegram_part.push_back(word);            
        }
        
        printf("received correct telegram %s, with %d elements\n", buffer, telegram_part.size());

        printf("regex recognized following parts:\n");

        for (size_t i = 0; i < telegram_part.size(); i++)
            std::cout <<"element "<<i<<" "<< telegram_part[i] << '\n';

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
                //free(buffer);
            }
        }
        printf("initialization done\n");

        switch (mode)
        {
        case UI_DOMAIN :
            printf("telegram requests UI_DOMAIN\n");
            /* stop wakeup thread if pending */
            if (wakeup_t.joinable())
                wakeup_t.join();

            /* start thread for wakeup functionality*/
            wakeup_t = std::thread(&UIHandle::look_for_web_field,this);
            break;
        case UI_LOGIN:
        case UI_PASSWORD:

            printf("telegram requests %s", (mode == UI_LOGIN) ? "UI_LOGIN" : "UI_PASSWORD");
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
                    printf("failed to enter %s credential\n", (mode == UI_LOGIN) ? "UI_LOGIN" : "UI_PASSWORD");

                    prepare_notification(UI_FAIL, telegram_part[1]);
                }
            }
            else
            {
                printf("wrong amount of elements regex found :%d :\n", telegram_part.size());
            }
            break;
        case UI_LOGPASS:
            printf("telegram requests UI_LOGPASS");
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
                        printf("failed to enter UI_PASSWORD credential\n");

                        prepare_notification(UI_FAIL, telegram_part[1]);
                    }

                }
                else
                {
                    printf("failed to enter UI_LOGIN credential\n");

                    prepare_notification(UI_FAIL, telegram_part[1]);
                }
            }
            else
            {
                printf("wrong amount of elements regex found :%d :\n", telegram_part.size());
            }
                

            break;
        case UI_DONE:
            break;
        case UI_NEW_CREDENTIAL:
            break;
        case UI_ERASE:
            break;
        case UI_MISSED:
            printf("telegram requests UI_MISSED\n");
            break;
        default:
            printf("unknown mode received %d\n", mode);
            break;
        }
    }
    else
    {
    printf("unknown message template has been received %s\n",buffer);
    }

    free(buffer);
    return ;
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
            printf(" searching url timeout\n");
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

            printf("which_element returned\n");

            if (element not_eq UI_ENUM::UI_UNKNOWN)
            {
                printf("before prepare notification\n");
                /* request for credentials*/
                prepare_notification(element, res.domain);
                printf("after prepare notification\n");
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
            printf(" initialization timeout\n");
            return false;
        }

        /* handle is not nullptr*/
        if (window_handle)
        {
            /* Window has title and is not closed/minimized */
            if (IsWindowVisible(window_handle) && !IsIconic(window_handle) && GetWindowTextLength(window_handle) > 0)
            {
                printf("elapsedTime :%d\n", elapsedTime);
                int c = GetWindowTextLength(window_handle);
                //printf("%d\n", c);
                LPWSTR pszMem = (LPWSTR)malloc(sizeof(LPWSTR) * (c + 1));
                GetWindowText(window_handle, pszMem, c + 1);
                wprintf(L"%s\n", pszMem);
                free(pszMem);

                break;
            }
        }

        HWND child_handl = nullptr; // always nullptr

        /* check if some window has className "Chrome_WidgetWin_1"*/
        window_handle = FindWindowEx(nullptr, child_handl, L"Chrome_WidgetWin_1", nullptr);
        if (!window_handle)
        {
            printf("handle from FindWindowEx is nullptr\n");
            return false;
        }
        
    }

    printf("%p\n", root.p);
    printf("%p\n", uia.p);

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
            printf("%s\n", page);

            if (lookup(page))
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
                printf("hit hashtable\n");
            }
            else
            {
                /* release memory if we don't have such domain in dictionary*/
                free(page);
                page = nullptr;

                printf("miss hashtable\n");
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
        printf("### root cannot find correct element\n");
        return (res=ERR);
    }
    int count = 0;
    arr->get_Length(&count);
    printf("editable_length:%d\n", count);

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
        printf("### pointer error: pane is nullptr\n");
        return (res = ERR);
    }
        
    //look for first UIA_EditControlTypeId under "Google Chrome" pane
    CComPtr<IUIAutomationElement> url;
    CComPtr<IUIAutomationCondition> url_cond;
    uia->CreatePropertyCondition(UIA_ControlTypePropertyId,
        CComVariant(UIA_EditControlTypeId), &url_cond);
    if FAILED(pane->FindFirst(TreeScope_Descendants, url_cond, &url))
    {
        printf("### pane cannot find correct element\n");
        return (res = ERR);
    }
        

    if (!url)
    {
        printf("### pointer error: url is nullptr\n");
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
        printf("### url cannot GetCurrentPropertyValue \n");
        return (res = ERR);
    }
    if (!var.bstrVal)
    {
        printf("### Variant string has nullpointer\n");
        return (res = ERR);
    }
    wprintf(L"find_url: %s\n", var.bstrVal);

    int length = SysStringLen(var.bstrVal);
    printf("%d\n", length);
    size_t t;
    char* mbstring = (char*)malloc(length + 1);

    wcstombs_s(&t, mbstring, length + 1, var.bstrVal, length);
    printf("%jd\n", t);
    printf("%s\n", mbstring);

    res.domain = domain_recognition(mbstring,req_dom);

    free(mbstring);
    //set new address ...
    IValueProvider* pattern = nullptr;
    //if (FAILED(pane->GetCurrentPattern(UIA_ValuePatternId, (IUnknown**)&pattern)))
    //    return false;
    if (FAILED(url->GetCurrentPattern(UIA_ValuePatternId, (IUnknown**)&pattern)))
    {
        printf("### url cannot GetCurrentPattern \n");
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
    struct nlist* map_entry = lookup(domain);
    if (!map_entry)
    {
        //TODO: create global log function ("tag","message")
        printf("#### struct nlist* allocation error: map_entry is nullptr \n");
        return nullptr;
    }

    /* default property for searching datafield*/
    PROPERTYID property = UIA_AutomationIdPropertyId;

    /* request for login field*/
    if (ui == UI_ENUM::UI_LOGIN)
    {
        var.bstrVal = SysAllocString(CA2W(map_entry->defn.login));
        wprintf(L"allocated String: %ls\n", var.bstrVal);
        var.vt = VT_BSTR;

        if (!var.bstrVal)
        {
            printf("#### string allocation error: var.bstrVal is nullptr \n");
            return nullptr;
        }
    }
    /* request for password field*/
    else if (ui == UI_ENUM::UI_PASSWORD)
    {
        printf("password property\n");
        var.boolVal = VARIANT_TRUE;
        var.vt = VT_BOOL;
        property = UIA_IsPasswordPropertyId;
    }
    /* wrong request*/
    else
    {
        printf("#### Wrong UI_ENUM \n");
        return nullptr;
    }

    res = uia->CreatePropertyCondition(property, var, &pane_cond);
    if FAILED(res)
    {
        printf("#### condition property creation error: CreatePropertyCondition() returns %08x \n", res);
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
        printf("#### finding error: FindFirst() returns %08x \n", res);
        pane_cond.Release();
        return nullptr;
    }

    if (!element)
    {
        printf("#### element allocation error: element is nullptr \n");
        return nullptr;
    }

    pane_cond.Release();
    CComBSTR name;
    element->get_CurrentAutomationId(&name);
    wprintf(L"name:%ls\n", (wchar_t*)name);
    SysFreeString(name);


    printf("%p\n", element);
    return element;
}

UI_ENUM UIHandle::which_element(const char* domain)
{
    // The root window has several childs, 
    // one of them is a "pane" named "Google Chrome"
    // This contains the toolbar. Find this "Google Chrome" element:
    
    UI_ENUM ret_val= UI_ENUM::UI_UNKNOWN;
    struct nlist* map_entry = lookup(domain);
    if (!map_entry)
    {
        printf("#### struct nlist* allocation error: map_entry is nullptr \n");
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
            var.bstrVal = SysAllocString(CA2W(map_entry->defn.login));
            wprintf(L"allocated String: %ls\n", var.bstrVal);
            var.vt = VT_BSTR;
            property = UIA_AutomationIdPropertyId;

            if (!var.bstrVal)
            {
                printf("#### string allocation error: var.bstrVal is nullptr \n");
                continue;
            }
        }
        else if (ui == UI_ENUM::UI_PASSWORD)
        {
            printf("password property\n");
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
                
            printf("#### finding error: FindFirst() returns %08x \n", res);
            pane_cond.Release();
            continue;
        }

        if (!element)
        {              
            printf("#### element allocation error: element is nullptr \n");
            continue;
        }

        pane_cond.Release();
        CComBSTR name;
        element->get_CurrentAutomationId(&name);
        wprintf(L"name:%ls\n", (wchar_t*)name);
        SysFreeString(name);
        printf("%p\n", element);
        
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
        printf("#### finding requested url error \n");
        return false;
    }

    CComPtr<IUIAutomationElement> elem = find_element(ui_mode, domain);

    if (!elem)
    {
        printf("#### element finding error: %s for [%s] domain has not been found \n", (ui_mode == 1) ? "login" : "password", domain);
        return false;
    }

    printf("%p\n", elem);

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
        printf("### conversion error: mbstowcs_s() returns %d  \n", res);
        return false;
    }

    printf("converted characters:%I64u\n", t);
    wprintf(L"%s\n", w_text);

    IValueProvider* pattern = nullptr;
    if (FAILED(elem->GetCurrentPattern(UIA_ValuePatternId, (IUnknown**)&pattern)))
        return false;

    if (!pattern)
    {
        printf("### pointer error: pattern is nullptr  \n");
        return false;
    }

    pattern->SetValue(w_text);
    //pattern->get_Value(&var.bstrVal);


    if FAILED(elem->GetCurrentPropertyValue(UIA_ValueValuePropertyId, &var))
    {
        printf("### GetCurrentPropertyValue error: cannot check inserted credential \n");
        return false;
    }
       
    if (!var.bstrVal)
    {
        printf("### string allocation memory error: !var.bstrVal is nullptr \n");
        return false;
    }

    wprintf(L"inserted content: %s\n", var.bstrVal);

    if (w_text != var.bstrVal)
    {
        printf("### set text is not same like get \n");
        //TODO: return fault if texts are not the same
        //return false;
    }
    
    SysFreeString(var.bstrVal);

    // Check if the element HasKeyboardFocus property
    VARIANT_BOOL hasKeyboardFocus;
    if FAILED(elem->GetCurrentPropertyValue(UIA_HasKeyboardFocusPropertyId, &var))
    {
        printf("### GetCurrentPropertyValue error: cannot check if element HasKeyboardFocus\n");
        //return false;
    }

    if (var.boolVal == VARIANT_TRUE) 
        printf("The element has keyboard focus.");
    else 
        printf("The element does not have keyboard focus.");
    
    pattern->Release();
    free(w_text);
    //SysFreeString(var.bstrVal);


    elem.Release();

    printf("finished succesfully\n");
    return true;
}