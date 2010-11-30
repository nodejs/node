/*
 * This file contains all winsock-related stuff.
 * Socketpair() for winsock is implemented here.
 * There are also functions to create a non-overlapped socket (which windows normally doesn't do)
 * and to create a socketpair that has one synchronous and one async socket.
 * Synchronous sockets are required because async sockets can't be used by child processes.
 */


#include <windows.h>
#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <ws2spi.h>
#include <platform_win32_winsock.h>


namespace node {


/*
 * Winsock version data goes here
 */
static WSAData winsock_info;


/*
 * Cache for WSAPROTOCOL_INFOW structures for protocols used in node
 * [0] TCP/IP
 * [1] UDP/IP
 * [2] TCP/IPv6
 * [3] UDP/IPv6
 */
static WSAPROTOCOL_INFOW proto_info_cache[4];


/*
 * Does the about the same as perror(), but for winsock errors
 */
void wsa_perror(const char *prefix) {
  DWORD errorno = WSAGetLastError();
  char *errmsg;

  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL, errorno, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&errmsg, 0, NULL);

  // FormatMessage messages include a newline character

  if (prefix) {
    fprintf(stderr, "%s: %s", prefix, errmsg);
  } else {
    fputs(errmsg, stderr);
  }
}


/*
 * Retrieves a pointer to a WSAPROTOCOL_INFOW structure
 * related to a certain winsock protocol from the cache
 */
inline static WSAPROTOCOL_INFOW *wsa_get_cached_proto_info(int af, int type, int proto) {
  assert(proto == IPPROTO_IP
      || proto == IPPROTO_TCP
      || proto == IPPROTO_UDP);

  switch (af) {
    case AF_INET:
      switch (type) {
        case SOCK_STREAM:
          return &proto_info_cache[0];
        case SOCK_DGRAM:
          return &proto_info_cache[1];
      }
      break;

    case AF_INET6:
      switch (type) {
        case SOCK_STREAM:
          return &proto_info_cache[2];
        case SOCK_DGRAM:
          return &proto_info_cache[3];
      }
      break;
  }

  WSASetLastError(WSAEPROTONOSUPPORT);
  return NULL;
}


/*
 * Creates a synchronous, non-overlapped socket.
 * (The sockets that are created with socket() or accept() are always in overlapped mode.)
 * Doubles the winsock api, e.g. returns a SOCKET handle, not an FD
 */
SOCKET wsa_sync_socket(int af, int type, int proto) {
  WSAPROTOCOL_INFOW *protoInfo = wsa_get_cached_proto_info(af, type, proto);
  if (protoInfo == NULL)
    return INVALID_SOCKET;

  return WSASocketW(af, type, proto, protoInfo, 0, 0);
}


/*
 * Create a socketpair using the protocol specified
 * This function uses winsock semantics, it returns SOCKET handles, not FDs
 * Currently supports TCP/IPv4 socket pairs only
 */
int wsa_socketpair(int af, int type, int proto, SOCKET sock[2]) {
  assert(af == AF_INET
      && type == SOCK_STREAM
      && (proto == IPPROTO_IP || proto == IPPROTO_TCP));

  SOCKET listen_sock;
  SOCKADDR_IN addr1;
  SOCKADDR_IN addr2;
  int addr1_len = sizeof (addr1);
  int addr2_len = sizeof (addr2);
  sock[1] = INVALID_SOCKET;
  sock[2] = INVALID_SOCKET;

  if ((listen_sock = socket(af, type, proto)) == INVALID_SOCKET)
    goto error;

  memset((void*)&addr1, 0, sizeof(addr1));
  addr1.sin_family = af;
  addr1.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr1.sin_port = 0;

  if (bind(listen_sock, (SOCKADDR*)&addr1, addr1_len) == SOCKET_ERROR)
    goto error;

  if (getsockname(listen_sock, (SOCKADDR*)&addr1, &addr1_len) == SOCKET_ERROR)
    goto error;

  if (listen(listen_sock, 1))
    goto error;

  if ((sock[0] = socket(af, type, proto)) == INVALID_SOCKET)
    goto error;

  if (connect(sock[0], (SOCKADDR*)&addr1, addr1_len))
    goto error;

  if ((sock[1] = accept(listen_sock, 0, 0)) == INVALID_SOCKET)
    goto error;

  if (getpeername(sock[0], (SOCKADDR*)&addr1, &addr1_len) == INVALID_SOCKET)
    goto error;

  if (getsockname(sock[1], (SOCKADDR*)&addr2, &addr2_len) == INVALID_SOCKET)
    goto error;

  if (addr1_len != addr2_len
      || addr1.sin_addr.s_addr != addr2.sin_addr.s_addr
      || addr1.sin_port        != addr2.sin_port)
    goto error;

  closesocket(listen_sock);

  return 0;

error:
  int error = WSAGetLastError();

  if (listen_sock != INVALID_SOCKET)
    closesocket(listen_sock);

  if (sock[0] != INVALID_SOCKET)
    closesocket(sock[0]);

  if (sock[1] != INVALID_SOCKET)
    closesocket(sock[1]);

  WSASetLastError(error);

  return SOCKET_ERROR;
}


/*
 * Create a sync-async socketpair using the protocol specified,
 * returning a synchronous socket and an asynchronous socket.
 * Upon completion asyncSocket is opened with the WSA_FLAG_OVERLAPPED flag set,
 * syncSocket won't have it set.
 * Currently supports TCP/IPv4 socket pairs only
 */
int wsa_sync_async_socketpair(int af, int type, int proto, SOCKET *syncSocket, SOCKET *asyncSocket) {
  assert(af == AF_INET
      && type == SOCK_STREAM
      && (proto == IPPROTO_IP || proto == IPPROTO_TCP));

  SOCKET listen_sock;
  SOCKET sock1 = INVALID_SOCKET;
  SOCKET sock2 = INVALID_SOCKET;
  SOCKADDR_IN addr1;
  SOCKADDR_IN addr2;
  int addr1_len = sizeof (addr1);
  int addr2_len = sizeof (addr2);

  if ((listen_sock = socket(af, type, proto)) == INVALID_SOCKET)
    goto error;

  memset((void*)&addr1, 0, sizeof(addr1));
  addr1.sin_family = af;
  addr1.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr1.sin_port = 0;

  if (bind(listen_sock, (SOCKADDR*)&addr1, addr1_len) == SOCKET_ERROR)
    goto error;

  if (getsockname(listen_sock, (SOCKADDR*)&addr1, &addr1_len) == SOCKET_ERROR)
    goto error;

  if (listen(listen_sock, 1))
    goto error;

  if ((sock1 = wsa_sync_socket(af, type, proto)) == INVALID_SOCKET)
    goto error;

  if (connect(sock1, (SOCKADDR*)&addr1, addr1_len))
    goto error;

  if ((sock2 = accept(listen_sock, 0, 0)) == INVALID_SOCKET)
    goto error;

  if (getpeername(sock1, (SOCKADDR*)&addr1, &addr1_len) == INVALID_SOCKET)
    goto error;

  if (getsockname(sock2, (SOCKADDR*)&addr2, &addr2_len) == INVALID_SOCKET)
    goto error;

  if (addr1_len != addr2_len
      || addr1.sin_addr.s_addr != addr2.sin_addr.s_addr
      || addr1.sin_port        != addr2.sin_port)
    goto error;

  closesocket(listen_sock);

  *syncSocket = sock1;
  *asyncSocket = sock2;

  return 0;

error:
  int error = WSAGetLastError();

  if (listen_sock != INVALID_SOCKET)
    closesocket(listen_sock);

  if (sock1 != INVALID_SOCKET)
    closesocket(sock1);

  if (sock2 != INVALID_SOCKET)
    closesocket(sock2);

  WSASetLastError(error);

  return SOCKET_ERROR;
}


/*
 * Retrieves a WSAPROTOCOL_INFOW structure for a certain protocol
 */
static void wsa_get_proto_info(int af, int type, int proto, WSAPROTOCOL_INFOW *target) {
  WSAPROTOCOL_INFOW *info_buffer = NULL;
  unsigned long info_buffer_length = 0;
  int protocol_count, i, error;

  if (WSCEnumProtocols(NULL, NULL, &info_buffer_length, &error) != SOCKET_ERROR) {
    error = WSAEOPNOTSUPP;
    goto error;
  }

  info_buffer = (WSAPROTOCOL_INFOW *)malloc(info_buffer_length);

  if ((protocol_count = WSCEnumProtocols(NULL, info_buffer, &info_buffer_length, &error)) == SOCKET_ERROR)
    goto error;

  for (i = 0; i < protocol_count; i++) {
    if (af == info_buffer[i].iAddressFamily
        && type == info_buffer[i].iSocketType
        && proto == info_buffer[i].iProtocol
        && info_buffer[i].dwServiceFlags1 & XP1_IFS_HANDLES) {
      memcpy(target, (WSAPROTOCOL_INFOW*)&info_buffer[i], sizeof(WSAPROTOCOL_INFOW));
      free(info_buffer);
      return;
    }
  }

  error = WSAEPROTONOSUPPORT;

error:
  WSASetLastError(error);
  wsa_perror("Error obtaining winsock protocol information");

  if (info_buffer != NULL) {
    free(info_buffer);
  }
}


/*
 * Initializes (fills) the WSAPROTOCOL_INFOW structure cache
 */
static void wsa_init_proto_info_cache() {
  WSAPROTOCOL_INFOW *cache = (WSAPROTOCOL_INFOW*)&proto_info_cache;

  wsa_get_proto_info(AF_INET,  SOCK_STREAM, IPPROTO_TCP, &proto_info_cache[0]);
  wsa_get_proto_info(AF_INET,  SOCK_DGRAM,  IPPROTO_UDP, &proto_info_cache[1]);
  wsa_get_proto_info(AF_INET6, SOCK_STREAM, IPPROTO_TCP, &proto_info_cache[2]);
  wsa_get_proto_info(AF_INET6, SOCK_DGRAM,  IPPROTO_UDP, &proto_info_cache[3]);
}


/*
 * Initializes winsock and winsock-related stuff
 */
void wsa_init() {
  WORD version = MAKEWORD(2, 2);
  if (WSAStartup(version, &winsock_info)) {
    wsa_perror("WSAStartup");
  }

  wsa_init_proto_info_cache();
}


} // namespace node
