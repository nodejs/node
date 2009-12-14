#include <node_net2.h>
#include <v8.h>

#include <node.h>

#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h> /* inet_pton */

#include <errno.h>



namespace node {

using namespace v8;

static Persistent<String> errno_symbol;
static Persistent<String> syscall_symbol;

static inline Local<Value> ErrnoException(int errorno, const char *syscall, const char *msg = "") {
  if (!msg[0]) msg = strerror(errorno);
  Local<Value> e = Exception::Error(String::NewSymbol(msg));
  Local<Object> obj = e->ToObject();
  obj->Set(errno_symbol, Integer::New(errorno));
  obj->Set(syscall_symbol, String::NewSymbol(syscall));
  return e;
}

// Creates a new non-blocking socket fd
// t.socket("TCP");
// t.socket("UNIX");
// t.socket("UDP");
static Handle<Value> Socket(const Arguments& args) {
  HandleScope scope;

  // default to TCP
  int domain = PF_INET6;
  int type = SOCK_STREAM;

  if (args[0]->IsString()) {
    String::Utf8Value t(args[0]->ToString());
    if (0 == strcasecmp(*t, "TCP")) {
      domain = PF_INET6;
      type = SOCK_STREAM;
    } else if (0 == strcasecmp(*t, "UDP")) {
      domain = PF_INET6;
      type = SOCK_DGRAM;
    } else if (0 == strcasecmp(*t, "UNIX")) {
      domain = PF_UNIX;
      type = SOCK_STREAM;
    } else {
      return ThrowException(Exception::Error(
            String::New("Unknown socket type.")));
    }
  }

  int fd = socket(domain, type, 0);

  if (fd < 0) return ThrowException(ErrnoException(errno, "socket"));

  int fcntl_errno;

  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    fcntl_errno = errno;
    close(fd);
    return ThrowException(ErrnoException(fcntl_errno, "fcntl"));
  }

  flags |= O_NONBLOCK;

  if (fcntl(fd, F_SETFL, flags) == -1) {
    fcntl_errno = errno;
    close(fd);
    return ThrowException(ErrnoException(fcntl_errno, "fcntl"));
  }

  return scope.Close(Integer::New(fd));
}

// 2 arguments means connect with unix
//   t.connect(fd, "/tmp/socket")
//
// 3 arguments means connect with TCP or UDP
//   t.connect(fd, "127.0.0.1", 80)
static Handle<Value> Connect(const Arguments& args) {
  HandleScope scope;

  if (!args[0]->IsInt32()) {
    return ThrowException(Exception::TypeError(
          String::New("First argument should be file descriptor")));
  }

  if (args.Length() < 2) {
    return ThrowException(Exception::TypeError(
          String::New("Must have at least two args")));
  }

  int fd = args[0]->Int32Value();

  struct sockaddr *addr;
  socklen_t addrlen;

  if (args.Length() == 2) {
    // UNIX
    String::Utf8Value path(args[1]->ToString());

    struct sockaddr_un un = {0};

    if (path.length() > sizeof un.sun_path) {
      return ThrowException(Exception::Error(String::New("Socket path too long")));
    }

    un.sun_family = AF_UNIX;
    strcpy(un.sun_path, *path);

    addr = (struct sockaddr*)&un;
    addrlen = path.length() + sizeof(un.sun_family);

  } else {
    // TCP or UDP
    String::Utf8Value ip(args[1]->ToString());
    int port = args[2]->Int32Value();
   
    struct sockaddr_in6 in6 = {0};

    char ipv6[255] = "::FFFF:";

    if (inet_pton(AF_INET, *ip, &(in6.sin6_addr)) > 0) {
      // If this is an IPv4 address then we need to change it to the
      // IPv4-mapped-on-IPv6 format which looks like 
      //  ::FFFF:<IPv4  address>
      // For more information see "Address Format" ipv6(7) and "BUGS" in
      // inet_pton(3) 
      strcat(ipv6, *ip);
    } else {
      strcpy(ipv6, *ip);
    }

    if (inet_pton(AF_INET6, ipv6, &(in6.sin6_addr)) <= 0) {
      return ThrowException(
        ErrnoException(errno, "inet_pton", "Invalid IP Address"));
    }

    in6.sin6_family = AF_INET6;
    in6.sin6_port = htons(port);  

    addr = (struct sockaddr*)&in6;
    addrlen = sizeof in6;
  }

  int r = connect(fd, addr, addrlen);

  if (r < 0 && errno != EINPROGRESS) {
    return ThrowException(ErrnoException(errno, "connect"));
  }

  return Undefined();
}


void InitNet2(Handle<Object> target) {
  HandleScope scope;

  NODE_SET_METHOD(target, "socket", Socket);
  NODE_SET_METHOD(target, "connect", Connect);
 
  errno_symbol = NODE_PSYMBOL("errno");
  syscall_symbol = NODE_PSYMBOL("syscall");
}

}  // namespace node
