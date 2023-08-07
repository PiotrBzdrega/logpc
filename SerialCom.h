#pragma once
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

/* typedef function wrapper to UIHandle callback*/
typedef std::function<void(uint8_t*, int)> UIHandleCb;

constexpr int BUFFER_LEN = 1024;

/* base on https://www.pololu.com/docs/0J73/15.6 
       nad http://www.egmont.com.pl/addi-data/instrukcje/standard_driver.pdf */
class SerialCom
{
private:
	/* COM ports higher than COM9 need the \\.\ prefix, which is written as
	   "\\\\.\\" in C because we need to escape the backslashes. */
	const char* device;;

	/* Choose the baud rate(bits per second) */
	uint32_t baud_rate;

	/* handle for serial port */
	HANDLE port;

	/* closed handle to not close twice */
	BOOL handle_closed = true;

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

	/* yields local time now */
	void print_timestamp(const char* tag);

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
	SerialCom() : SerialCom("\\\\.\\COM7", 9600) {};
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

