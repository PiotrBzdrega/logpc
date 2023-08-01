#include "UIHandle.h"

UIHandle::UIHandle()
{
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

    CoUninitialize();

    release_dict();
}

void UIHandle::initialize_instance() 
{
    start this thread also in find_url

    while (true)
    {
        HWND child_handl = nullptr; // always nullptr
        hwnd = FindWindowEx(nullptr, child_handl, L"Chrome_WidgetWin_1", nullptr);
        if (!hwnd)
            return;

        /* Window has title and is not closed/minimized */
        if (IsWindowVisible(hwnd) && !IsIconic(hwnd) && GetWindowTextLength(hwnd) > 0)
        {
            int c = GetWindowTextLength(hwnd);
            printf("%d\n", c);
            LPWSTR pszMem = (LPWSTR)malloc(sizeof(wchar_t) * (c+1));
            GetWindowText(hwnd, pszMem, c+1);
            wprintf(L"%s\n", pszMem);
            free(pszMem);

            break;
        }
    }


        if SUCCEEDED(uia->ElementFromHandle(hwnd, &root))
        {
            ;
        }
    
}

void UIHandle::valid_elem_cb(uint8_t* buffer, size_t size, bool (*func_ptr)(uint8_t*, size_t))
{
    (*func_ptr)(buffer,size);
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

bool UIHandle::domain_recognition(const char* url)
{
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
                hit = true;
                printf("hit hashtable\n");
            }
            else
            {
                printf("miss hashtable\n");
            }


            //printf("%s\n", strcmp(page, "yandex")==0 ? "success" : "miss");
            first_found = true;
            free(page);
            page = nullptr;
        }

    }
    return hit;
}

bool UIHandle::find_url()
{
    // The root window has several childs, 
    // one of them is a "pane" named "Google Chrome"
    // This contains the toolbar. Find this "Google Chrome" pane:
    CComPtr<IUIAutomationElement> pane;
    CComPtr<IUIAutomationCondition> pane_cond;
    uia->CreatePropertyCondition(UIA_ControlTypePropertyId,
        CComVariant(UIA_PaneControlTypeId), &pane_cond);

    CComPtr<IUIAutomationElementArray> arr;
    if FAILED(root->FindAll(TreeScope_Children, pane_cond, &arr))
        return false;
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
        return false;
    //look for first UIA_EditControlTypeId under "Google Chrome" pane
    CComPtr<IUIAutomationElement> url;
    CComPtr<IUIAutomationCondition> url_cond;
    uia->CreatePropertyCondition(UIA_ControlTypePropertyId,
        CComVariant(UIA_EditControlTypeId), &url_cond);
    if FAILED(pane->FindFirst(TreeScope_Descendants, url_cond, &url))
        return false;

    if (!url)
    {
        printf("### pointer error: url is nullptr  \n");
        return false;
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
        return false;
    if (!var.bstrVal)
        return false;
    wprintf(L"find_url: %s\n", var.bstrVal);

    int length = SysStringLen(var.bstrVal);
    printf("%d\n", length);
    size_t t;
    char* mbstring = (char*)malloc(length + 1);

    wcstombs_s(&t, mbstring, length + 1, var.bstrVal, length);
    printf("%jd\n", t);
    printf("%s\n", mbstring);

    bool hit = domain_recognition(mbstring);


    /* invoke callback with found*/
    valid_elem_cb((uint8_t*)mbstring, static_cast<size_t>(length) + 1, callback);

    free(mbstring);
    //set new address ...
    IValueProvider* pattern = nullptr;
    //if (FAILED(pane->GetCurrentPattern(UIA_ValuePatternId, (IUnknown**)&pattern)))
    //    return false;
    if (FAILED(url->GetCurrentPattern(UIA_ValuePatternId, (IUnknown**)&pattern)))
        return false;
    //pattern->SetValue(L"google.com");
    pattern->Release();

    return hit;
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
        printf("#### struct nlist* allocation error: map_entry is nullptr \n");
        return nullptr;
    }

    var.bstrVal = SysAllocString(CA2W(map_entry->defn.data[ui]));
    wprintf(L"allocated String: %ls\n", var.bstrVal);
    var.vt = VT_BSTR;

    if (!var.bstrVal)
    {
        printf("#### string allocation error: var.bstrVal is nullptr \n");
        return nullptr;
    }

    res = uia->CreatePropertyCondition(UIA_AutomationIdPropertyId, var, &pane_cond);
    if FAILED(res)
    {
        printf("#### condition property creation error: CreatePropertyCondition() returns %08x \n", res);
        pane_cond.Release();
        return nullptr;
    }

    SysFreeString(var.bstrVal);

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

    pattern->SetValue(w_text);
    pattern->Release();
    free(w_text);


    elem.Release();

    printf("finished succesfully\n");
    return true;
}