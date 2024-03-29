﻿#pragma once
#include <stdint.h>
#include <cstdio>
#include <Windows.h>
#include <fileapi.h>
#include <handleapi.h>
#include <WinBase.h>
#include <future>       // std::async, std::future, std::launch
#include <thread> 
#include <mutex>
#include <condition_variable> // std::condition_variable
#include <functional>
#include "spdlog/spdlog.h" //loging
#include "spdlog/sinks/stdout_color_sinks.h"
#include "cfgmgr32.h"
#include <tchar.h>
#include <ntddser.h>
#include <initguid.h>
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <devpkey.h>

#include <bluetoothapis.h>
#include <iostream>
#include <string_view>
#include <vector>

/* typedef function wrapper to UIHandle callback*/
typedef std::function<void(std::string)> UIHandleCb;

constexpr int BUFFER_LEN = 1024;

/* base on https://www.pololu.com/docs/0J73/15.6 
       and http://www.egmont.com.pl/addi-data/instrukcje/standard_driver.pdf */
class SerialCom
{
private:
	/* COM ports higher than COM9 need the \\.\ prefix, which is written as
	   "\\\\.\\" in C because we need to escape the backslashes. */
	const char* device;

	/* Choose the baud rate(bits per second) */
	uint32_t baud_rate;

	/* handle for serial port */
	HANDLE port;

	DWORD old_dwModemStatus;

	/* closed handle to not close twice */
	BOOL handle_closed = true;

	//TODO:clean buffer after read, block access to shared data
	/* read messages are stored in this message after read_port() */
	uint8_t read_buffer[BUFFER_LEN];

	/* callback from UIHandleCb for event: new telegram to be processed*/
	UIHandleCb callback;

	/* close handle with message*/
	void close_handle();
	
	/* display communication event  thrown by WaitCommEvent() */
	BOOL comm_status(DWORD state);

	/* complementary funtion to readin operation */
	BOOL withdraw_buffer(SSIZE_T len);

	/* return last error in serial port*/
	std::string last_error();

	/* thread for event feedback */
	std::thread event_t;

	/* thread for port handling */
	std::thread handle_t;

	/* thread for UIHandle callback */
	std::thread callback_t;

	/* task for port handling thread */
	void connection_loop();

public:

	SerialCom(const char* device_name, uint32_t com_speed);

	//TODO: from where device name comes from and what basically defines this speed
	SerialCom() : SerialCom( /*"\\\\.\\COM7"*/ "\\\\?\\BTHENUM#{00001101-0000-1000-8000-00805F9B34FB}_LOCALMFG&005D#8&116EB950&0&7C9EBD4B402E_C00000000#{86e0d1e0-8089-11d0-9ce4-08003e301f73}"
		/*"BTHENUM\Dev_7C9EBD4B402E\8&52e0ba6&0&BluetoothDevice_7C9EBD4B402E"*/, 9600) {};
	~SerialCom();

	//TODO:put to private and create getter 
	/* current state if communication port is opened*/
	std::mutex mtx;
	std::condition_variable cv;
	bool port_down = true;

	///* verify port state*/
	bool IsOpened() { return !port_down; }

	/*Reads bytes from the serial port.
	  Returns after all the desired bytes have been read, or if there is a
	  timeout or other error.
	  Returns the number of bytes successfully read into the buffer, or -1 if
	  there was an error reading.*/ 
	SSIZE_T	read_port();

	/* assign callback to class */
	bool add_callback(UIHandleCb callback);

	/* callback function to wait for communication events */
	void event_callback();

	/* Writes bytes to the serial port, returning 0 on success and -1 on failure.*/
	void write_port(uint8_t* buffer, size_t size);

	/* print message to sderr*/
	void print_error(const char* context);

	/* opens the specified serial port, configures its timeouts, and sets its
	   baud rate.  Returns a handle on success, or INVALID_HANDLE_VALUE on failure. */
	int open_serial_port();

};

