#ifndef NODE_PLATFORM_WIN32_WINSOCK_H_
#define NODE_PLATFORM_WIN32_WINSOCK_H_

#include <windows.h>
#include <winsock.h>

namespace node {


void wsa_init();

void wsa_perror(const char* prefix = "");

BOOL wsa_disconnect_ex(SOCKET socket, OVERLAPPED *overlapped, DWORD flags, DWORD reserved);

SOCKET wsa_sync_socket(int af, int type, int proto);
int wsa_socketpair(int af, int type, int proto, SOCKET sock[2]);
int wsa_sync_async_socketpair(int af, int type, int proto, SOCKET *syncSocket, SOCKET *asyncSocket);


} // namespace node

#endif  // NODE_PLATFORM_WIN32_WINSOCK_H_
