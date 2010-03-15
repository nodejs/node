#include <node_stdio.h>
#include <node_events.h>
#include <coupling.h>

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

using namespace v8;
namespace node {


static struct coupling *stdin_coupling = NULL;
static struct coupling *stdout_coupling = NULL;

static int stdin_fd = -1;
static int stdout_fd = -1;


static Local<Value> errno_exception(int errorno) {
  Local<Value> e = Exception::Error(String::NewSymbol(strerror(errorno)));
  Local<Object> obj = e->ToObject();
  obj->Set(String::NewSymbol("errno"), Integer::New(errorno));
  return e;
}


/* STDERR IS ALWAY SYNC ALWAYS UTF8 */
static Handle<Value>
WriteError (const Arguments& args)
{
  HandleScope scope;

  if (args.Length() < 1)
    return Undefined();

  String::Utf8Value msg(args[0]->ToString());

  ssize_t r;
  size_t written = 0;
  while (written < msg.length()) {
    r = write(STDERR_FILENO, (*msg) + written, msg.length() - written);
    if (r < 0) {
      if (errno == EAGAIN || errno == EIO) {
        usleep(100);
        continue;
      }
      return ThrowException(errno_exception(errno));
    }
    written += (size_t)r;
  }

  return Undefined();
}


static inline int SetNonblock(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) return -1;

  int r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  if (r == -1) return -1;

  return 0;
}


static Handle<Value> OpenStdin(const Arguments& args) {
  HandleScope scope;

  if (stdin_fd >= 0) {
    return ThrowException(Exception::Error(String::New("stdin already open")));
  }

  if (isatty(STDIN_FILENO)) {
    // XXX selecting on tty fds wont work in windows.
    // Must ALWAYS make a coupling on shitty platforms.
    stdin_fd = STDIN_FILENO;
  } else {
    stdin_coupling = coupling_new_pull(STDIN_FILENO);
    stdin_fd = coupling_nonblocking_fd(stdin_coupling);
  }
  SetNonblock(stdin_fd);

  return scope.Close(Integer::New(stdin_fd));
}


void Stdio::Flush() {
  if (stdout_fd >= 0) {
    close(stdout_fd);
    stdout_fd = -1;
  }

  if (stdout_coupling) {
    coupling_join(stdout_coupling);
    coupling_destroy(stdout_coupling);
    stdout_coupling = NULL;
  }
}


void Stdio::Initialize(v8::Handle<v8::Object> target) {
  HandleScope scope;

  if (isatty(STDOUT_FILENO)) {
    // XXX selecting on tty fds wont work in windows.
    // Must ALWAYS make a coupling on shitty platforms.
    stdout_fd = STDOUT_FILENO;
  } else {
    stdout_coupling = coupling_new_push(STDOUT_FILENO);
    stdout_fd = coupling_nonblocking_fd(stdout_coupling);
  }
  SetNonblock(stdout_fd);

  target->Set(String::NewSymbol("stdoutFD"), Integer::New(stdout_fd));

  NODE_SET_METHOD(target, "writeError", WriteError);
  NODE_SET_METHOD(target, "openStdin", OpenStdin);
}


}  // namespace node
