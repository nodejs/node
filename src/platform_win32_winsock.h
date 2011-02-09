#ifndef NODE_PLATFORM_WIN32_WINSOCK_H_
#define NODE_PLATFORM_WIN32_WINSOCK_H_

#include <platform_win32.h>

#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <ws2spi.h>

namespace node {

void wsa_init();

void wsa_perror(const char* prefix = "");

SOCKET wsa_sync_socket(int af, int type, int proto);

int wsa_socketpair(int af, int type, int proto, SOCKET sock[2]);
int wsa_sync_async_socketpair(int af, int type, int proto, SOCKET *syncSocket, SOCKET *asyncSocket);


} // namespace node

#endif  // NODE_PLATFORM_WIN32_WINSOCK_H_
