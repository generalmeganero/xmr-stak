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
#include "json_console.hpp"

#include <mutex>
#include <atomic>
#include <condition_variable>
#include <thread>
#include <string>
#include <map>
#include <vector>
#include "xmrstak/misc/utility.hpp"
#include "xmrstak/misc/environment.hpp"
#include "xmrstak/misc/console.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <stdio.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define PORT    "32001" /* Port to listen on */
#define BACKLOG 10      /* Passed to listen() */

//json_console* json_console:: oInst = NULL;

json_console::json_console() {

}
int json_console::broadcast_message(const char *msg) {
	
	write_buffers_mutex.lock();
	for (std::map<SOCKET, std::string>::iterator iter = write_socket_buffers.begin(); iter != write_socket_buffers.end(); ++iter)
	{
		SOCKET k = iter->first;
		std::string buffer = iter->second;
		write_socket_buffers[k] = buffer + msg;
	}
	write_buffers_mutex.unlock();
	write_notify.notify_one();
	return 0;

}

json_console* json_console::inst()
{
	auto& env = xmrstak::environment::inst();
	if (env.pJsonConsole == nullptr)
		env.pJsonConsole = new json_console;
	return env.pJsonConsole;
};

void json_console::json_console_write_thread() {
	/* Set up the fd_set */
	/* Main loop */
	
	while (1) {
		std::unique_lock<std::mutex> lck(write_mutex);
		write_notify.wait(lck);
		write_buffers_mutex.lock();
		if (write_socket_buffers.size() == 0) {
			write_buffers_mutex.unlock();
			continue;
		}
		write_buffers_mutex.unlock();

		SOCKET s;
		socks_mutex.lock();
		writesocks = socks;
		socks_mutex.unlock();
		FD_CLR(server_sock, &writesocks);
		if (select(maxsock + 1, NULL, &writesocks, NULL, NULL) == SOCKET_ERROR) {
			perror("select");
			printf("Error in write select\n");
			continue;
		}
		for (s = 0; s <= maxsock; s++) {
			if (FD_ISSET(s, &writesocks)) {
				write_buffers_mutex.lock();
				std::map<SOCKET, std::string>::iterator it = write_socket_buffers.find(s);
				if (it != write_socket_buffers.end()) {
					std::string buffer = it->second;
					int wrote = send(s, buffer.c_str(), buffer.length(), 0);
					if (wrote >= 0)
						write_socket_buffers[s] = buffer.substr(wrote, std::string::npos);
				}
				write_buffers_mutex.unlock();
			}
		}
	}
}
int json_console::handle_command(std::string command)
{
	printer::inst()->print_msg(L0, "Ignoring command '%s'", command.c_str());
	return 0;
}
void json_console::json_console_read_thread() {
	/* Set up the fd_set */
	socks_mutex.lock();
	FD_ZERO(&socks);
	FD_SET(server_sock, &socks);
	socks_mutex.unlock();

	maxsock = server_sock;

	/* Main loop */
	while (1) {
		SOCKET s;
		socks_mutex.lock();
		readsocks = socks;
		socks_mutex.unlock();

		if (select(maxsock + 1, &readsocks, NULL, NULL, NULL) == SOCKET_ERROR) {
			perror("select");
			WSACleanup();
			printf("Error in read select\n");
		}
		for (s = 0; s <= maxsock; s++) {
			if (FD_ISSET(s, &readsocks)) {
				if (s == server_sock) {
					/* New connection */
					
					SOCKET newsock;
					struct sockaddr_in their_addr;
					size_t size = sizeof(struct sockaddr_in);

					newsock = accept(server_sock, (struct sockaddr*)&their_addr, (int*)&size);
					if (newsock == INVALID_SOCKET) {
						perror("accept");
					}
					else {
						printer::inst()->print_msg(L0, "Got a connection from %s on port %d\n",inet_ntoa(their_addr.sin_addr), htons(their_addr.sin_port));
						
						socks_mutex.lock();
						FD_SET(newsock, &socks);
						socks_mutex.unlock();
						if (newsock > maxsock) {
							maxsock = newsock;
						}
						write_socket_buffers[newsock] = "";
					}
				}
				else {
					/* Handle read or disconnection */
					//handle(s, &socks);
					
					char rbuf[1600];
					memset(rbuf, 0, 1600);

					std::string buffer("");
					std::map<SOCKET, std::string>::iterator it = read_socket_buffers.find(s);
					if (it != read_socket_buffers.end()) {
						buffer = it->second;
					}

					int len = recv(s, rbuf, 1599, 0);
					if (len <= 0) {
						read_socket_buffers.erase(s);

						write_buffers_mutex.lock();
						write_socket_buffers.erase(s);
						write_buffers_mutex.unlock();

						socks_mutex.lock();
						FD_CLR(s, &socks);
						socks_mutex.unlock();

						closesocket(s);
						continue;
					}
					buffer.append(rbuf);

					std::vector<std::string> lines;
					xmrstak::split(buffer, "\n", lines);

					for (std::vector<std::string>::size_type i = 0; i < lines.size() - 1; i++) {
						handle_command(lines[i]);
					}
					//printf("remaining: %s\n", lines[lines.size() - 1].c_str());


					read_socket_buffers[s] = buffer;


				}
			}
		}
	}
}

int json_console::init() {
	WORD wVersion = MAKEWORD(2, 2);
	WSADATA wsaData;
	int iResult;

	struct addrinfo hints, *res;
	int reuseaddr = 1; /* True */

	/* Initialise Winsock */
	if ((iResult = WSAStartup(wVersion, &wsaData)) != 0) {
		printf("WSAStartup failed: %d\n", iResult);
		return 1;
	}

	/* Get the address info */
	ZeroMemory(&hints, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	if (getaddrinfo(NULL, PORT, &hints, &res) != 0) {
		perror("getaddrinfo");
		return 1;
	}

	/* Create the socket */
	server_sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (server_sock == INVALID_SOCKET) {
		perror("socket");
		WSACleanup();
		return 1;
	}

	/* Enable the socket to reuse the address */
	if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuseaddr, sizeof(int)) == SOCKET_ERROR) {
		perror("setsockopt");
		WSACleanup();
		return 1;
	}

	/* Bind to the address */
	if (bind(server_sock, res->ai_addr, res->ai_addrlen) == SOCKET_ERROR) {
		perror("bind");
		WSACleanup();
		return 1;
	}

	/* Listen */
	if (listen(server_sock, BACKLOG) == SOCKET_ERROR) {
		perror("listen");
		WSACleanup();
		return 1;
	}

	freeaddrinfo(res);



	consoleRdThd = std::thread(&json_console::json_console_read_thread, this);
	consoleWrThd = std::thread(&json_console::json_console_write_thread, this);

}




json_console::~json_console() {
	/* Clean up */
	closesocket(server_sock);
	WSACleanup();

}