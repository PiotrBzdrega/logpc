#include "UIHandle.h"

UIHandle::UIHandle()
{
    /* initialize domains credentials*/
    init_dict(ui_element);

    //TODO: comment
    HRESULT res = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if SUCCEEDED(uia.CoCreateInstance(CLSID_CUIAutomation))
    {
    /* start initialization thread */
    init_t = std::thread(&UIHandle::initialize_instance, this);
    }
}

UIHandle::~UIHandle()
{
    /* stop initialization thread */
    if (init_t.joinable())
        init_t.join();

    /* release smart pointer*/
    uia.Release();

    //TODO: comment
    CoUninitialize();

    /* release domains credentials*/
    release_dict();
}

bool UIHandle::add_callback(SerialComCb callback)
{
    /* assign to class callback*/
    this->callback = callback; 
    return true;
}

bool UIHandle::process_data(uint8_t* buffer, size_t size)
{
    //reinterpret uint8_t* to char* is followed by convert the char* to std::string
    std::string input(reinterpret_cast<char*>(buffer));

    // Regular expression pattern to match words between commas and first digit.
    std::regex pattern("^(\\d)(?:,(\\w+))?(?:,(\\w+))?$");


    /* check if pattern covers input */
    if (std::regex_match(input, pattern))
    {
        std::stringstream ss(input);
        std::string word;

        std::vector <std::string> telegram_part;
        /* split words in match*/
        while (std::getline(ss, word, ',')) 
        {
            printf("%s\n", word);
            /* add new element to list */
            telegram_part.push_back(word);            
        }
        
        /* according regex pattern 0 element is a digit example: char '2' -> int 50. 50-48('0')=2*/
        UI_ENUM mode = static_cast<UI_ENUM>(telegram_part[0][0]-'0');

        switch (mode)
        {
        case UI_DOMAIN :

            break;
        case UI_LOGIN:
        case UI_PASSWORD:

            /* following telegram should contain 3 elements in telegram*/
            if (telegram_part.size()==3)
            {
                /* enter found credential*/
                bool res = credential(mode, telegram_part[1].c_str(), telegram_part[2].c_str());
            }

            break;
        case UI_DONE:

        case UI_NEW_CREDENTIAL:
       
        case UI_DELETE_ALL:

        case UI_MISSED:

        default:
            break;
        }


    }

    return false;
}

void UIHandle::finding_loop()
{
    /* result of function invocation*/
    ret_url res = { nullptr,1 };
    while (res.val)
    {
        /*check if message is comming */
        {
            std::unique_lock<decltype(mtx_msg)> lk(mtx_msg);

            /* wait for mutex with timeout*/
            if (cv_msg.wait_for(lk, std::chrono::seconds(20), [this]() {return !msg_pending; }));
            else
            {
                printf("timeout for mtx_msg\n");
                /* if timeout happened reset msg_pending */
                msg_pending = false;
                /* finish task if there is no response from uC*/
                break;
            }
        }

        /* search w/o concrete domain comparison */
        res=find_url("");
        /* got known domain */
        if (res.val)
        {

            UI_ENUM element = find_element(UI_ENUM::UI_UNKNOWN, res.domain);
            /* invoke callback with found*/
                if (callback)
                {
                    callback(static_cast<uint8_t*>(res.domain),)
                }
            valid_elem_cb((uint8_t*)mbstring, (length) + 1, callback);
        }
    }

    /* out of while loop, something went wrong*/
    return initialize_instance();
}

void UIHandle::initialize_instance() 
{

    while (true)
    {
        HWND child_handl = nullptr; // always nullptr

        /* check if some window has className "Chrome_WidgetWin_1"*/
        hwnd = FindWindowEx(nullptr, child_handl, L"Chrome_WidgetWin_1", nullptr);
        if (!hwnd)
            return;

        /* Window has title and is not closed/minimized */
        if (IsWindowVisible(hwnd) && !IsIconic(hwnd) && GetWindowTextLength(hwnd) > 0)
        {
            int c = GetWindowTextLength(hwnd);
            printf("%d\n", c);
            LPWSTR pszMem = (LPWSTR)malloc(sizeof(LPWSTR) * (c+1));
            GetWindowText(hwnd, pszMem, c+1);
            wprintf(L"%s\n", pszMem);
            free(pszMem);

            break;
        }
    }


        if SUCCEEDED(uia->ElementFromHandle(hwnd, &root))
        {
            return finding_loop();
        }
    
}

void UIHandle::valid_elem_cb(uint8_t* buffer, size_t size)
{

    callback(buffer, size);
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
                if (req_dom!='\0')
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
                /* release momory if we don't have such domain in dictionary*/
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
                return UI_ENUM::UI_UNKNOWN;
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

        res = uia->CreatePropertyCondition(UIA_AutomationIdPropertyId, var, &pane_cond);
        if FAILED(res)
        {
            /* there is missing LOGIN field*/
            if (ui == UI_ENUM::UI_LOGIN)
                continue;

            printf("#### condition property creation error: CreatePropertyCondition() returns %08x \n", res);
            pane_cond.Release();
            SysFreeString(var.bstrVal);
            return UI_ENUM::UI_UNKNOWN;
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
            /* there is missing LOGIN field*/
            if (ui == UI_ENUM::UI_LOGIN)
                continue;

            printf("#### finding error: FindFirst() returns %08x \n", res);
            pane_cond.Release();
            return UI_ENUM::UI_UNKNOWN;
        }

        if (!element)
        {
            /* there is missing LOGIN field*/
            if (ui == UI_ENUM::UI_LOGIN)
                continue;

            printf("#### element allocation error: element is nullptr \n");
            return UI_ENUM::UI_UNKNOWN;
        }

        pane_cond.Release();
        CComBSTR name;
        element->get_CurrentAutomationId(&name);
        wprintf(L"name:%ls\n", (wchar_t*)name);
        SysFreeString(name);
        printf("%p\n", element);
        element.Release();

        /* check if field is filled out or empty*/
        if FAILED(element->GetCurrentPropertyValue(UIA_ValueValuePropertyId, &var))
            return UI_ENUM::UI_UNKNOWN;
        if (!var.bstrVal)
            return UI_ENUM::UI_UNKNOWN;

        wprintf(L"content of field: %s\n", var.bstrVal);

        /* login field is empty*/
        if ((var.bstrVal[0] == L'\0' && ui == UI_ENUM::UI_LOGIN) ||
            ui == UI_ENUM::UI_PASSWORD)
        {
            /* save found UI*/
            ret_val = static_cast<UI_ENUM>(ui);
            SysFreeString(var.bstrVal);
            break;
        }        
        SysFreeString(var.bstrVal);
    }

    return ret_val;
}

bool UIHandle::credential(UI_ENUM ui_mode, const char* domain, const char* credential)
{

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
    pattern->Release();
    free(w_text);


    elem.Release();

    printf("finished succesfully\n");
    return true;
}