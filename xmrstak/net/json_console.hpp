/*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
* Additional permission under GNU GPL version 3 section 7
*
* If you modify this Program, or any covered work, by linking or combining
* it with OpenSSL (or a modified version of that library), containing parts
* covered by the terms of OpenSSL License and SSLeay License, the licensors
* of this Program grant you additional permission to convey the resulting work.
*
*/


#include <mutex>
#include <atomic>
#include <condition_variable>
#include <thread>
#include <string>
#include <map>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <stdio.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

class json_console
{
public:

	static json_console* inst();

	json_console();
	int init();
	~json_console();
	int broadcast_message(const char *msg);
	

private:
	SOCKET server_sock;
	std::thread consoleRdThd;
	std::thread consoleWrThd;
	void json_console_read_thread();
	void json_console_write_thread();
	int handle_command(std::string command);

	fd_set socks;
	fd_set readsocks;
	fd_set writesocks;
	SOCKET maxsock;

	std::map<SOCKET, std::string> read_socket_buffers;
	std::map<SOCKET, std::string> write_socket_buffers;

	std::mutex write_buffers_mutex;
	std::mutex socks_mutex;
	std::condition_variable write_notify;
	std::mutex write_mutex;
	

};