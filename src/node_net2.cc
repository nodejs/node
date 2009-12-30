#include <node_net2.h>
#include <v8.h>

#include <node.h>
#include <node_buffer.h>

#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h> /* inet_pton */

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <sys/ioctl.h>

#ifdef __linux__
# include <linux/sockios.h> /* For the SIOCINQ / FIONREAD ioctl */
#endif
/* Non-linux platforms like OS X define this ioctl elsewhere */
#ifndef FIONREAD
#include <sys/filio.h>
#endif

#include <errno.h>

namespace node {

using namespace v8;

static Persistent<String> errno_symbol;
static Persistent<String> syscall_symbol;

static Persistent<String> fd_symbol;
static Persistent<String> remote_address_symbol;
static Persistent<String> remote_port_symbol;
static Persistent<String> address_symbol;
static Persistent<String> port_symbol;

#define FD_ARG(a)                                                     \
  if (!(a)->IsInt32()) {                                              \
    return ThrowException(Exception::TypeError(                       \
          String::New("Bad file descriptor argument")));              \
  }                                                                   \
  int fd = (a)->Int32Value();

static inline Local<Value> ErrnoException(int errorno,
                                          const char *syscall,
                                          const char *msg = "") {
  if (!msg[0]) msg = strerror(errorno);
  Local<Value> e = Exception::Error(String::NewSymbol(msg));
  Local<Object> obj = e->ToObject();
  obj->Set(errno_symbol, Integer::New(errorno));
  obj->Set(syscall_symbol, String::NewSymbol(syscall));
  return e;
}

static inline bool SetCloseOnExec(int fd) {
  return (fcntl(fd, F_SETFD, FD_CLOEXEC) != -1);
}

static inline bool SetNonBlock(int fd) {
  return (fcntl(fd, F_SETFL, O_NONBLOCK) != -1);
}

static inline bool SetSockFlags(int fd) {
  return SetNonBlock(fd) && SetCloseOnExec(fd);
}

// Creates nonblocking pipe
static Handle<Value> Pipe(const Arguments& args) {
  HandleScope scope;
  int fds[2];

  if (pipe(fds) < 0) return ThrowException(ErrnoException(errno, "pipe"));

  if (!SetSockFlags(fds[0]) || !SetSockFlags(fds[1])) {
    int fcntl_errno = errno;
    close(fds[0]);
    close(fds[1]);
    return ThrowException(ErrnoException(fcntl_errno, "fcntl"));
  }

  Local<Array> a = Array::New(2);
  a->Set(Integer::New(0), Integer::New(fds[0]));
  a->Set(Integer::New(1), Integer::New(fds[1]));
  return scope.Close(a);
}

// Creates nonblocking socket pair
static Handle<Value> SocketPair(const Arguments& args) {
  HandleScope scope;

  int fds[2];

  // XXX support SOCK_DGRAM?
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
    return ThrowException(ErrnoException(errno, "socketpair"));
  }

  if (!SetSockFlags(fds[0]) || !SetSockFlags(fds[1])) {
    int fcntl_errno = errno;
    close(fds[0]);
    close(fds[1]);
    return ThrowException(ErrnoException(fcntl_errno, "fcntl"));
  }

  Local<Array> a = Array::New(2);
  a->Set(Integer::New(0), Integer::New(fds[0]));
  a->Set(Integer::New(1), Integer::New(fds[1]));
  return scope.Close(a);
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

  if (!SetSockFlags(fd)) {
    int fcntl_errno = errno;
    close(fd);
    return ThrowException(ErrnoException(fcntl_errno, "fcntl"));
  }

  return scope.Close(Integer::New(fd));
}


// NOT AT ALL THREAD SAFE - but that's okay for node.js
// (yes this is all to avoid one small heap alloc)
static struct sockaddr *addr;
static socklen_t addrlen;
static struct sockaddr_un un;
static struct sockaddr_in6 in6;
static inline Handle<Value> ParseAddressArgs(Handle<Value> first,
                                             Handle<Value> second,
                                             struct in6_addr default_addr
                                            ) {
  if (first->IsString() && second->IsUndefined()) {
    // UNIX
    String::Utf8Value path(first->ToString());

    if (path.length() > sizeof un.sun_path) {
      return Exception::Error(String::New("Socket path too long"));
    }

    memset(&un, 0, sizeof un);
    un.sun_family = AF_UNIX;
    strcpy(un.sun_path, *path);

    addr = (struct sockaddr*)&un;
    addrlen = path.length() + sizeof(un.sun_family);

  } else {
    // TCP or UDP
    int port = first->Int32Value();
    memset(&in6, 0, sizeof in6);
    if (!second->IsString()) {
      in6.sin6_addr = default_addr;
    } else {
      String::Utf8Value ip(second->ToString());

      char ipv6[255] = "::FFFF:";

      if (inet_pton(AF_INET, *ip, &(in6.sin6_addr)) > 0) {
        // If this is an IPv4 address then we need to change it
        // to the IPv4-mapped-on-IPv6 format which looks like
        //        ::FFFF:<IPv4  address>
        // For more information see "Address Format" ipv6(7) and
        // "BUGS" in inet_pton(3)
        strcat(ipv6, *ip);
      } else {
        strcpy(ipv6, *ip);
      }

      if (inet_pton(AF_INET6, ipv6, &(in6.sin6_addr)) <= 0) {
        return ErrnoException(errno, "inet_pton", "Invalid IP Address");
      }
    }

    in6.sin6_family = AF_INET6;
    in6.sin6_port = htons(port);  

    addr = (struct sockaddr*)&in6;
    addrlen = sizeof in6;
  }
  return Handle<Value>();
}


// Bind with UNIX 
//   t.bind(fd, "/tmp/socket")
// Bind with TCP
//   t.bind(fd, 80, "192.168.11.2")
//   t.bind(fd, 80)
static Handle<Value> Bind(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 2) {
    return ThrowException(Exception::TypeError(
          String::New("Must have at least two args")));
  }

  FD_ARG(args[0])

  Handle<Value> error = ParseAddressArgs(args[1], args[2], in6addr_any);
  if (!error.IsEmpty()) return ThrowException(error);

  int flags = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void *)&flags, sizeof(flags));

  int r = bind(fd, addr, addrlen);

  if (r < 0) {
    return ThrowException(ErrnoException(errno, "bind"));
  }

  return Undefined();
}


static Handle<Value> Close(const Arguments& args) {
  HandleScope scope;

  FD_ARG(args[0])

  if (0 > close(fd)) {
    return ThrowException(ErrnoException(errno, "close"));
  }

  return Undefined();
}

// t.shutdown(fd, "read");      -- SHUT_RD
// t.shutdown(fd, "write");     -- SHUT_WR
// t.shutdown(fd, "readwrite"); -- SHUT_RDWR
// second arg defaults to "write".
static Handle<Value> Shutdown(const Arguments& args) {
  HandleScope scope;

  FD_ARG(args[0])

  int how = SHUT_WR;

  if (args[1]->IsString()) {
    String::Utf8Value t(args[1]->ToString());
    if (0 == strcasecmp(*t, "write")) {
      how = SHUT_WR;
    } else if (0 == strcasecmp(*t, "read")) {
      how = SHUT_RD;
    } else if (0 == strcasecmp(*t, "readwrite")) {
      how = SHUT_RDWR;
    } else {
      return ThrowException(Exception::Error(String::New(
            "Unknown shutdown method. (Use 'read', 'write', or 'readwrite'.)")));
    }
  }

  if (0 > shutdown(fd, how)) {
    return ThrowException(ErrnoException(errno, "shutdown"));
  }

  return Undefined();
}


// Connect with unix
//   t.connect(fd, "/tmp/socket")
//
// Connect with TCP or UDP
//   t.connect(fd, 80, "192.168.11.2")
//   t.connect(fd, 80, "::1")
//   t.connect(fd, 80)
//  the third argument defaults to "::1"
static Handle<Value> Connect(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 2) {
    return ThrowException(Exception::TypeError(
          String::New("Must have at least two args")));
  }

  FD_ARG(args[0])

  Handle<Value> error = ParseAddressArgs(args[1], args[2], in6addr_loopback);
  if (!error.IsEmpty()) return ThrowException(error);

  int r = connect(fd, addr, addrlen);

  if (r < 0 && errno != EINPROGRESS) {
    return ThrowException(ErrnoException(errno, "connect"));
  }

  return Undefined();
}


static Handle<Value> GetSockName(const Arguments& args) {
  HandleScope scope;

  FD_ARG(args[0])

  struct sockaddr_storage address_storage;
  socklen_t len = sizeof(struct sockaddr_storage);

  int r = getsockname(fd, (struct sockaddr *) &address_storage, &len);

  if (r < 0) {
    return ThrowException(ErrnoException(errno, "getsockname"));
  }

  Local<Object> info = Object::New();

  if (address_storage.ss_family == AF_INET6) {
    struct sockaddr_in6 *a = (struct sockaddr_in6*)&address_storage;

    char ip[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &(a->sin6_addr), ip, INET6_ADDRSTRLEN);

    int port = ntohs(a->sin6_port);

    info->Set(address_symbol, String::New(ip));
    info->Set(port_symbol, Integer::New(port));
  }

  return scope.Close(info);
}


static Handle<Value> Listen(const Arguments& args) {
  HandleScope scope;

  FD_ARG(args[0])
  int backlog = args[1]->IsInt32() ? args[1]->Int32Value() : 128;

  if (0 > listen(fd, backlog)) {
    return ThrowException(ErrnoException(errno, "listen"));
  }


  return Undefined();
}


// var peerInfo = t.accept(server_fd);
//
//   peerInfo.fd
//   peerInfo.remoteAddress
//   peerInfo.remotePort
//
// Returns a new nonblocking socket fd. If the listen queue is empty the
// function returns null (wait for server_fd to become readable and try
// again)
static Handle<Value> Accept(const Arguments& args) {
  HandleScope scope;

  FD_ARG(args[0])

  struct sockaddr_storage address_storage;
  socklen_t len = sizeof(struct sockaddr_storage);

  int peer_fd = accept(fd, (struct sockaddr*) &address_storage, &len);

  if (peer_fd < 0) {
    if (errno == EAGAIN) return scope.Close(Null());
    return ThrowException(ErrnoException(errno, "accept"));
  }

  if (!SetSockFlags(peer_fd)) {
    int fcntl_errno = errno;
    close(peer_fd);
    return ThrowException(ErrnoException(fcntl_errno, "fcntl"));
  }

  Local<Object> peer_info = Object::New();

  peer_info->Set(fd_symbol, Integer::New(peer_fd));

  if (address_storage.ss_family == AF_INET6) {
    struct sockaddr_in6 *a = (struct sockaddr_in6*)&address_storage;

    char ip[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &(a->sin6_addr), ip, INET6_ADDRSTRLEN);

    int port = ntohs(a->sin6_port);

    peer_info->Set(remote_address_symbol, String::New(ip));
    peer_info->Set(remote_port_symbol, Integer::New(port));
  }

  return scope.Close(peer_info);
}


static Handle<Value> SocketError(const Arguments& args) {
  HandleScope scope;

  FD_ARG(args[0])

  int error;
  socklen_t len = sizeof(int);
  int r = getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len);

  if (r < 0) {
    return ThrowException(ErrnoException(errno, "getsockopt"));
  }

  return scope.Close(Integer::New(error)); 
}

//  var bytesRead = t.read(fd, buffer, offset, length);
//  returns null on EAGAIN or EINTR, raises an exception on all other errors
//  returns 0 on EOF.
static Handle<Value> Read(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 4) {
    return ThrowException(Exception::TypeError(
          String::New("Takes 4 parameters")));
  }

  FD_ARG(args[0])

  if (!IsBuffer(args[1])) { 
    return ThrowException(Exception::TypeError(
          String::New("Second argument should be a buffer")));
  }

  struct buffer * buffer = BufferUnwrap(args[1]);

  size_t off = args[2]->Int32Value();
  if (buffer_p(buffer, off) == NULL) {
    return ThrowException(Exception::Error(
          String::New("Offset is out of bounds")));
  }

  size_t len = args[3]->Int32Value();
  if (buffer_remaining(buffer, off) < len) {
    return ThrowException(Exception::Error(
          String::New("Length is extends beyond buffer")));
  }

  ssize_t bytes_read = read(fd,
                            buffer_p(buffer, off),
                            buffer_remaining(buffer, off));

  if (bytes_read < 0) {
    if (errno == EAGAIN || errno == EINTR) return Null();
    return ThrowException(ErrnoException(errno, "read"));
  }

  return scope.Close(Integer::New(bytes_read));
}

//  var bytesWritten = t.write(fd, buffer, offset, length);
//  returns null on EAGAIN or EINTR, raises an exception on all other errors
static Handle<Value> Write(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 4) {
    return ThrowException(Exception::TypeError(
          String::New("Takes 4 parameters")));
  }

  FD_ARG(args[0])

  if (!IsBuffer(args[1])) { 
    return ThrowException(Exception::TypeError(
          String::New("Second argument should be a buffer")));
  }

  struct buffer * buffer = BufferUnwrap(args[1]);

  size_t off = args[2]->Int32Value();
  char *p = buffer_p(buffer, off);
  if (p == NULL) {
    return ThrowException(Exception::Error(
          String::New("Offset is out of bounds")));
  }

  size_t len = args[3]->Int32Value();
  size_t remaining = buffer_remaining(buffer, off);
  if (remaining < len) {
    return ThrowException(Exception::Error(
          String::New("Length is extends beyond buffer")));
  }

  ssize_t written = write(fd, p, len);

  if (written < 0) {
    if (errno == EAGAIN || errno == EINTR) return Null();
    return ThrowException(ErrnoException(errno, "write"));
  }

  return scope.Close(Integer::New(written));
}


// Probably only works for Linux TCP sockets?
// Returns the amount of data on the read queue.
static Handle<Value> ToRead(const Arguments& args) {
  HandleScope scope;

  FD_ARG(args[0])

  int value;
  int r = ioctl(fd, FIONREAD, &value);

  if (r < 0) {
    return ThrowException(ErrnoException(errno, "ioctl"));
  }

  return scope.Close(Integer::New(value));
}


// G E T A D D R I N F O

struct resolve_request {
  Persistent<Function> cb;
  struct addrinfo *address_list;
  int ai_family; // AF_INET or AF_INET6
  char hostname[1];
};


static int AfterResolve(eio_req *req) {
  ev_unref(EV_DEFAULT_UC);

  struct resolve_request * rreq = (struct resolve_request *)(req->data);

  HandleScope scope;
  Local<Value> argv[1];

  if (req->result != 0) {
    if (req->result == EAI_NODATA) {
      argv[0] = Array::New();
    } else {
      argv[0] = ErrnoException(req->result,
                               "getaddrinfo",
                               gai_strerror(req->result));
    }
  } else {
    struct addrinfo *address;
    int n = 0;

    for (address = rreq->address_list; address; address = address->ai_next) { n++; }

    Local<Array> results = Array::New(n);

    char ip[INET6_ADDRSTRLEN];
    const char *addr;

    n = 0;
    address = rreq->address_list;
    while (address) {
      assert(address->ai_socktype == SOCK_STREAM);
      assert(address->ai_family == AF_INET || address->ai_family == AF_INET6);
      addr = ( address->ai_family == AF_INET
             ? (char *) &((struct sockaddr_in *) address->ai_addr)->sin_addr
             : (char *) &((struct sockaddr_in6 *) address->ai_addr)->sin6_addr
             );
      const char *c = inet_ntop(address->ai_family, addr, ip, INET6_ADDRSTRLEN);
      Local<String> s = String::New(c);
      results->Set(Integer::New(n), s);

      n++;
      address = address->ai_next;
    }

    argv[0] = results;
  }

  TryCatch try_catch;

  rreq->cb->Call(Context::GetCurrent()->Global(), 1, argv);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }

  if (rreq->address_list) freeaddrinfo(rreq->address_list);
  rreq->cb.Dispose(); // Dispose of the persistent handle
  free(rreq);

  return 0;
}


static int Resolve(eio_req *req) {
  // Note: this function is executed in the thread pool! CAREFUL
  struct resolve_request * rreq = (struct resolve_request *) req->data;

  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = rreq->ai_family;
  hints.ai_socktype = SOCK_STREAM;

  req->result = getaddrinfo((char*)rreq->hostname,
                            NULL,
                            &hints,
                            &(rreq->address_list));
  return 0;
}


static Handle<Value> GetAddrInfo(const Arguments& args) {
  HandleScope scope;

  String::Utf8Value hostname(args[0]->ToString());

  int type = args[1]->Int32Value();
  int fam = AF_INET;
  switch (type) {
    case 4:
      fam = AF_INET;
      break;
    case 6:
      fam = AF_INET6;
      break;
    default:
      return ThrowException(Exception::TypeError(
            String::New("Second argument must be an integer 4 or 6")));
  }

  if (!args[2]->IsFunction()) {
    return ThrowException(Exception::TypeError(
          String::New("Thrid argument must be a callback")));
  }

  Local<Function> cb = Local<Function>::Cast(args[2]);

  struct resolve_request *rreq = (struct resolve_request *)
    calloc(1, sizeof(struct resolve_request) + hostname.length());

  if (!rreq) {
    V8::LowMemoryNotification();
    return ThrowException(Exception::Error(
          String::New("Could not allocate enough memory")));
  }

  strcpy(rreq->hostname, *hostname);
  rreq->cb = Persistent<Function>::New(cb);
  rreq->ai_family = fam;

  // For the moment I will do DNS lookups in the eio thread pool. This is
  // sub-optimal and cannot handle massive numbers of requests.
  //
  // (One particularly annoying problem is that the pthread stack size needs
  // to be increased dramatically to handle getaddrinfo() see X_STACKSIZE in
  // wscript ).
  //
  // In the future I will move to a system using c-ares:
  // http://lists.schmorp.de/pipermail/libev/2009q1/000632.html
  eio_custom(Resolve, EIO_PRI_DEFAULT, AfterResolve, rreq);

  // There will not be any active watchers from this object on the event
  // loop while getaddrinfo() runs. If the only thing happening in the
  // script was this hostname resolution, then the event loop would drop
  // out. Thus we need to add ev_ref() until AfterResolve().
  ev_ref(EV_DEFAULT_UC);

  return Undefined();
}


static Handle<Value> NeedsLookup(const Arguments& args) {
  HandleScope scope;

  if (args[0]->IsNull() || args[0]->IsUndefined()) return False();

  String::Utf8Value s(args[0]->ToString());

  // avoiding buffer overflows in the following strcat
  // 2001:0db8:85a3:08d3:1319:8a2e:0370:7334
  // 39 = max ipv6 address.
  if (s.length() > INET6_ADDRSTRLEN) return True();

  struct sockaddr_in6 a;

  if (inet_pton(AF_INET, *s, &(a.sin6_addr)) > 0) return False();
  if (inet_pton(AF_INET6, *s, &(a.sin6_addr)) > 0) return False();

  char ipv6[255] = "::FFFF:";
  strcat(ipv6, *s);
  if (inet_pton(AF_INET6, ipv6, &(a.sin6_addr)) > 0) return False();

  return True();
}


void InitNet2(Handle<Object> target) {
  HandleScope scope;

  NODE_SET_METHOD(target, "write", Write);
  NODE_SET_METHOD(target, "read", Read);

  NODE_SET_METHOD(target, "socket", Socket);
  NODE_SET_METHOD(target, "close", Close);
  NODE_SET_METHOD(target, "shutdown", Shutdown);
  NODE_SET_METHOD(target, "pipe", Pipe);
  NODE_SET_METHOD(target, "socketpair", SocketPair);

  NODE_SET_METHOD(target, "connect", Connect);
  NODE_SET_METHOD(target, "bind", Bind);
  NODE_SET_METHOD(target, "listen", Listen);
  NODE_SET_METHOD(target, "accept", Accept);
  NODE_SET_METHOD(target, "socketError", SocketError);
  NODE_SET_METHOD(target, "toRead", ToRead);
  NODE_SET_METHOD(target, "getsocksame", GetSockName);
  NODE_SET_METHOD(target, "getaddrinfo", GetAddrInfo);
  NODE_SET_METHOD(target, "needsLookup", NeedsLookup);

  target->Set(String::NewSymbol("EINPROGRESS"), Integer::New(EINPROGRESS));
  target->Set(String::NewSymbol("EINTR"), Integer::New(EINTR));
  target->Set(String::NewSymbol("EACCES"), Integer::New(EACCES));
  target->Set(String::NewSymbol("EPERM"), Integer::New(EPERM));
  target->Set(String::NewSymbol("EADDRINUSE"), Integer::New(EADDRINUSE));
  target->Set(String::NewSymbol("ECONNREFUSED"), Integer::New(ECONNREFUSED));

  errno_symbol          = NODE_PSYMBOL("errno");
  syscall_symbol        = NODE_PSYMBOL("syscall");
  fd_symbol             = NODE_PSYMBOL("fd");
  remote_address_symbol = NODE_PSYMBOL("remoteAddress");
  remote_port_symbol    = NODE_PSYMBOL("remotePort");
  address_symbol        = NODE_PSYMBOL("address");
  port_symbol           = NODE_PSYMBOL("port");
}

}  // namespace node
