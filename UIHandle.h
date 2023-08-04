#pragma once
#include <thread>
#include <UIAutomation.h>
#include <AtlBase.h>
#include <thread> 
#include <mutex>
#include <condition_variable>
#include "crc.h"                //hashmap
#include <functional>
#include <regex>
#include <sstream>

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

/* typedef function wrapper to SerialCom callback*/
typedef std::function<bool(uint8_t*, int)> SerialComCb;

/* result struct for find url function*/
typedef struct ret_url
{
	char* domain;
	bool val;
};

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

};

class UIHandle
{
private:

	/* Handle to Window */
	HWND hwnd;

	/* smart pointer for Component Object Model of User Interface on Windows */
	CComPtr<IUIAutomation> uia;

	/* smart pointer for Component Object Model of individual UI Element */
	CComPtr<IUIAutomationElement> root;

	/* window api initialization thread  */
	std::thread init_t;

	/* mutex wait for telegram from esp*/
	std::mutex mtx_msg;
	std::condition_variable cv_msg;
	bool msg_pending = false;


	/* callback from SerialCom for event: new telegram to be send*/
	SerialComCb callback;

	/* callback function for new element appears*/
	void valid_elem_cb(uint8_t* buffer, size_t size);

	/* finding url task for thread*/
	void finding_loop();

	/* initialization task for thread*/
	void initialize_instance();

	/* search for AutomationID on found domain*/
	UI_ENUM which_element(const char* domain);

	/* recognize web domain of currently opened tab in Chrome */
	ret_url find_url(const char* req_dom);

	/* determine if domain header is known */
	char* domain_recognition(const char* url, const char* req_dom);

	/* pressed ENTER button implicitly */
	bool accept_credenetial();

	/* enter credential in Chrome*/
	bool credential(UI_ENUM ui_mode, const char* domain, const char* credential);

	/**/
	CComPtr<IUIAutomationElement> find_element(UI_ENUM ui, const char* domain);
public:

	UIHandle();
	~UIHandle();

	/* process data received from peripheral*/
	bool process_data(uint8_t* buffer, size_t size);

	/* assign callback to class */
	bool add_callback(SerialComCb callback);

};

