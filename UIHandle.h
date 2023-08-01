#pragma once
#include <thread>
#include <UIAutomation.h>
#include <AtlBase.h>
#include <thread> 
#include <mutex>
#include <condition_variable>
#include "crc.h"                //hashmap

/*Telegram enumeration*/
typedef enum UI_ENUM
{
	UI_UNKNOWN = -1,
	UI_DOMAIN = 0,
	UI_LOGIN = 1,
	UI_PASSWORD = 2,
	UI_DONE = 3,
	UI_NEW_CREDENTIAL = 4,
	UI_DELETE_ALL = 5,
	UI_MISSED = 6
};

/* typedef function pointer for callback*/
typedef bool (*func_ptr)(uint8_t* buffer, size_t size);

class UIHandle
{
private:

	/* Handle to Window */
	HWND hwnd;

	/* smart pointer for Component Object Model of User Interface on Windows */
	CComPtr<IUIAutomation> uia;

	/* smart pointer for Component Object Model of individual UI Element */
	CComPtr<IUIAutomationElement> root;

	/* domain recognition thread */
	std::thread domain_t;

	/* window api initialization thread  */
	std::thread init_t;

	/* mutex wait for telegram from esp*/
	std::mutex mtx_msg;
	std::condition_variable cv_msg;
	bool msg_comming = false;

	/* callback for event: found valid element*/
	func_ptr callback;

	
	void initialize_instance();

	/* recognize web domain of currently opened tab in Chrome */
	bool find_url();

	/* determine if domain header is known */
	bool domain_recognition(const char* url);

	/* pressed ENTER button implicitly */
	bool accept_credenetial();

	/* enter credential in Chrome*/
	bool credential(UI_ENUM ui_mode, const char* domain, const char* credential);

	/**/
	CComPtr<IUIAutomationElement> find_element(UI_ENUM ui, const char* domain);
public:

	UIHandle();
	~UIHandle();

	/* callback function for new element appears*/
	void valid_elem_cb(uint8_t* buffer, size_t size, bool (*func_ptr)(uint8_t*, size_t));
};

