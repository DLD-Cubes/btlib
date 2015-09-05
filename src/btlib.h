/*
 * btlib.h
 *
 *  Created on: Sep 5, 2015
 *      Author: bkemper
 */

#ifndef BTLIB_H_
#define BTLIB_H_

#include <functional>
#include <string>
#include <atomic>
#include <thread>
#include <vector>
#include <bluetooth/bluetooth.h>

namespace BTLib
{

class BTScanner
{
public:
	BTScanner();
	BTScanner(bdaddr_t *);
	virtual ~BTScanner();

	bool init();

	void register_device_rssi_update(std::function<void(bdaddr_t, int8_t)> callback);

	std::string get_device_name(const bdaddr_t &bdaddr);

	void start_scanning();

	void stop_scanning();

private:
	void scan();

	int sock, dev_id;
	bdaddr_t *bdaddr;
	std::vector<std::function<void(bdaddr_t, int8_t)> > rssi_callbacks;
	std::thread scanning_thread;
	std::atomic_bool thread_stop_scanning;
};
}



#endif /* BTLIB_H_ */
