#include <node_stdio.h>
#include <node_events.h>

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

using namespace v8;
namespace node {


static int stdout_flags = -1;
static int stdin_flags = -1;


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
      return ThrowException(ErrnoException(errno, "write"));
    }
    written += (size_t)r;
  }

  return Undefined();
}


static Handle<Value> OpenStdin(const Arguments& args) {
  HandleScope scope;

  if (isatty(STDIN_FILENO)) {
    // XXX selecting on tty fds wont work in windows.
    // Must ALWAYS make a coupling on shitty platforms.
    stdin_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (stdin_flags == -1) {
      // TODO DRY
      return ThrowException(Exception::Error(String::New("fcntl error!")));
    }

    int r = fcntl(STDIN_FILENO, F_SETFL, stdin_flags | O_NONBLOCK);
    if (r == -1) {
      // TODO DRY
      return ThrowException(Exception::Error(String::New("fcntl error!")));
    }
  }

  return scope.Close(Integer::New(STDIN_FILENO));
}

static Handle<Value>
IsStdinBlocking (const Arguments& args)
{
  HandleScope scope;
  return scope.Close(Boolean::New(isatty(STDIN_FILENO)));
}

static Handle<Value>
IsStdoutBlocking (const Arguments& args)
{
  HandleScope scope;
  bool tty = isatty(STDOUT_FILENO);
  return scope.Close(Boolean::New(!tty));
}


void Stdio::Flush() {
  if (stdin_flags != -1) {
    fcntl(STDIN_FILENO, F_SETFL, stdin_flags & ~O_NONBLOCK);
  }

  if (STDOUT_FILENO >= 0) {
    if (stdout_flags != -1) {
      fcntl(STDOUT_FILENO, F_SETFL, stdout_flags & ~O_NONBLOCK);
    }

    close(STDOUT_FILENO);
  }
}


void Stdio::Initialize(v8::Handle<v8::Object> target) {
  HandleScope scope;

  if (isatty(STDOUT_FILENO)) {
    // XXX selecting on tty fds wont work in windows.
    // Must ALWAYS make a coupling on shitty platforms.
    stdout_flags = fcntl(STDOUT_FILENO, F_GETFL, 0);
    int r = fcntl(STDOUT_FILENO, F_SETFL, stdout_flags | O_NONBLOCK);
  }

  target->Set(String::NewSymbol("stdoutFD"), Integer::New(STDOUT_FILENO));

  NODE_SET_METHOD(target, "writeError", WriteError);
  NODE_SET_METHOD(target, "openStdin", OpenStdin);
  NODE_SET_METHOD(target, "isStdoutBlocking", IsStdoutBlocking);
  NODE_SET_METHOD(target, "isStdinBlocking", IsStdinBlocking);
}


}  // namespace node
