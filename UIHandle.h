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
#include <chrono>
#include <iostream>
#include <fstream>
#include "spdlog/spdlog.h" //loging
#include "spdlog/sinks/stdout_color_sinks.h"
#include "external/nlohmann/json.hpp" //json parser

/*Telegram enumeration*/
typedef enum UI_ENUM
{
	UI_UNKNOWN = -1,
	UI_DOMAIN = 0,
	UI_LOGIN = 1,
	UI_PASSWORD = 2,
	UI_LOGPASS = 3,
	UI_DONE = 4,
	UI_NEW_CREDENTIAL = 5,
	UI_ERASE = 6,
	UI_MISSED = 7,
	UI_FAIL = 8,
};

/* typedef function wrapper to SerialCom callback*/
typedef std::function<void(uint8_t*, int)> SerialComCb;

/* result struct for find url function*/
typedef struct ret_url
{
	char* domain;
	bool val;
}ret_url;



class UIHandle
{
private:

	/* smart pointer for Component Object Model of User Interface on Windows */
	CComPtr<IUIAutomation> uia;

	/* smart pointer for Component Object Model of individual UI Element */
	CComPtr<IUIAutomationElement> root;

	/* handle to web window */
	HWND window_handle;

	/* SerialCom callback thread */
	std::thread callback_t;

	/* wake-up callback thread */
	std::thread wakeup_t;

	/* mutex wait for telegram from esp*/
	std::mutex mtx_msg;
	std::condition_variable cv_msg;
	bool msg_pending = false;

	/* buffer to send telegram to SerialCom*/
	uint8_t telegram[1024]{ '\0' };

	/* callback from SerialCom for event: new telegram to be send*/
	SerialComCb callback;

	/* finding url task for thread*/
	void look_for_web_field();

	/* initialization task for thread*/
	bool initialize_instance();

	/* search for AutomationID on found domain*/
	UI_ENUM which_element(const char* domain);

	/* recognize web domain of currently opened tab in Chrome */
	ret_url find_url(const char* req_dom);

	/* determine if domain header is known */
	char* domain_recognition(const char* url, const char* req_dom);

	/* pressed ENTER button implicitly */
	bool accept_credenetial();

	/* concatenate arguments for telegram, return length of msg*/
	int create_message(UI_ENUM mode, std::string domain);

	/* prepare interface for SerialCom callback*/
	void prepare_notification(UI_ENUM mode, std::string domain);

	/* enter credential in Chrome*/
	bool credential(UI_ENUM ui_mode, const char* domain, const char* credential);

	/* verify if on passed domain there is requested element*/
	CComPtr<IUIAutomationElement> find_element(UI_ENUM ui, const char* domain);

public:

	UIHandle();
	~UIHandle();

	/* process data received from peripheral*/
	void process_data(std::string authorization);

	/* assign callback to class */
	bool add_callback(SerialComCb callback);

};

