#include "btlib.h"

#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>

namespace BTLib
{

BTScanner::BTScanner(): sock(0), dev_id(0), bdaddr(NULL) {}

BTScanner::BTScanner(bdaddr_t *addr): sock(0), dev_id(0), bdaddr(addr) {}

BTScanner::~BTScanner()
{
	close(this->sock);
}

bool BTScanner::init()
{
	this->dev_id = hci_get_route(this->bdaddr);
	this->sock = hci_open_dev(dev_id);
	if (this->dev_id < 0 || this->sock < 0) {
		perror("Can't open socket");
		return false;
	}
	return true;
}

void BTScanner::scan()
{
	struct hci_filter flt;
	hci_filter_clear(&flt);
	hci_filter_set_ptype(HCI_EVENT_PKT, &flt);
	hci_filter_set_event(EVT_INQUIRY_RESULT_WITH_RSSI, &flt);
	hci_filter_set_event(EVT_INQUIRY_COMPLETE, &flt);
	if (setsockopt(this->sock, SOL_HCI, HCI_FILTER, &flt, sizeof(flt)) < 0) {
		perror("Can't set HCI filter");
		return;
	}

	write_inquiry_mode_cp wicp = { 1 /* mode */};
	if (hci_send_cmd(this->sock, OGF_HOST_CTL, OCF_WRITE_INQUIRY_MODE,
			WRITE_INQUIRY_MODE_CP_SIZE, &wicp) < 0) {
		perror("Can't set inquiry mode");
		return;
	}

	inquiry_cp icp = {{ 0x33, 0x8b, 0x9e }, // lap
			0x1, // length
			0x0}; // num_rsp

	while (!thread_stop_scanning.load()) {
		if (hci_send_cmd(this->sock, OGF_LINK_CTL, OCF_INQUIRY, INQUIRY_CP_SIZE, &icp)
				< 0) {
			perror("Can't start inquiry");
			return;
		}

		struct pollfd p;
		p.fd = this->sock;
		p.events = POLLIN | POLLERR | POLLHUP;
		p.revents = 0;

		/* poll the BT device for an event */
		unsigned char buf[HCI_MAX_EVENT_SIZE] = {0}, *ptr = 0;
		int results, i, len;
		if (poll(&p, 1, -1) > 0) {
			len = read(this->sock, buf, sizeof(buf));

			if (len < 0)
				continue;
			else if (len == 0)
				break;

			hci_event_hdr *hdr = (void *) (buf + 1);
			ptr = buf + (1 + HCI_EVENT_HDR_SIZE);

			results = ptr[0];

			switch (hdr->evt) {

			case EVT_INQUIRY_RESULT_WITH_RSSI:
				for (i = 0; i < results; i++) {
					inquiry_info_with_rssi *info_rssi =
							(void *) ptr + (sizeof(*info_rssi) * i) + 1;
					for (auto callback : rssi_callbacks)
						callback(info_rssi->bdaddr, info_rssi->rssi);

				}
				break;

			case EVT_INQUIRY_COMPLETE:
				break;
			}
		}
	}
}

std::string BTScanner::get_device_name(const bdaddr_t &addr)
{
	char name[256] = {0};
	if (hci_read_remote_name(sock, &addr, sizeof(name), name, 0) < 0)
	        strcpy(name, "[unknown]");

	return std::string(name);
}

void BTScanner::start_scanning()
{
	thread_stop_scanning.store(false);
	scanning_thread = std::thread(&BTScanner::scan, this);
	std::cout << "[btlib] Started scanning..." << std::endl;
}

void BTScanner::stop_scanning()
{
	std::cout << "[btlib] Stopping scan!" << std::endl;
	thread_stop_scanning.store(true);
	scanning_thread.join();
	std::cout << "[btlib] Finished scanning!" << std::endl;
}

void BTScanner::register_device_rssi_update(std::function<void(bdaddr_t, int8_t)> callback)
{
	rssi_callbacks.push_back(callback);
}


}

BTLib::BTScanner scanner;

void print_callback(bdaddr_t addr, int8_t rssi)
{
	 char addr_str[18];
	 ba2str(&addr, addr_str);

	 std::cout << "[btlib] " << scanner.get_device_name(addr) <<
			 ", ID: " << addr_str << ", RSSI: " << (int)rssi << std::endl;
}

int main() {
	scanner.init();

	scanner.register_device_rssi_update(print_callback);
	scanner.start_scanning();

	sleep(30);

	scanner.stop_scanning();

    return 0;
}
