#include <node.h>
#include <node_stdio.h>

#include <v8.h>

#include <errno.h>
#include <io.h>

#include <platform_win32.h>

using namespace v8;
namespace node {

#define THROW_ERROR(msg) \
    return ThrowException(Exception::Error(String::New(msg)));
#define THROW_BAD_ARGS \
    return ThrowException(Exception::TypeError(String::New("Bad argument")));

/*
 * Flush stdout and stderr on node exit
 * Not necessary on windows, so a no-op
 */
void Stdio::Flush() {
}


/*
 * STDERR should always be blocking
 */
static Handle<Value> WriteError(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1)
    return Undefined();

  String::Utf8Value msg(args[0]->ToString());

  fprintf(stderr, "%s", reinterpret_cast<char*>(*msg));

  return Undefined();
}


static Handle<Value> IsATTY(const Arguments& args) {
  HandleScope scope;
  int fd = args[0]->IntegerValue();
  DWORD result;
  int r = GetConsoleMode((HANDLE)_get_osfhandle(fd), &result);
  return scope.Close(r ? True() : False());
}


/* Whether stdio is currently in raw mode */
/* -1 means that it has not been set */
static int rawMode = -1;


static void setRawMode(int newMode) {
  DWORD flags;
  BOOL result;

  if (newMode != rawMode) {
    if (newMode) {
      // raw input
      flags = ENABLE_WINDOW_INPUT;
    } else {
      // input not raw, but still processing enough messages to make the
      // tty watcher work (this mode is not the windows default)
      flags = ENABLE_ECHO_INPUT | ENABLE_INSERT_MODE | ENABLE_LINE_INPUT |
          ENABLE_PROCESSED_INPUT | ENABLE_WINDOW_INPUT;
    }

    result = SetConsoleMode((HANDLE)_get_osfhandle(STDIN_FILENO), flags);
    if (result) {
      rawMode = newMode;
    }
  }
}


static Handle<Value> SetRawMode(const Arguments& args) {
  HandleScope scope;

  int newMode = !args[0]->IsFalse();
  setRawMode(newMode);

  if (newMode != rawMode) {
    return ThrowException(ErrnoException(GetLastError(), "EnableRawMode"));
  }

  return scope.Close(rawMode ? True() : False());
}



void Stdio::DisableRawMode(int fd) {
  if (rawMode == 1)
    setRawMode(0);
}


static Handle<Value> OpenStdin(const Arguments& args) {
  HandleScope scope;
  setRawMode(0); // init into nonraw mode
  return scope.Close(Integer::New(STDIN_FILENO));
}


static Handle<Value> IsStdinBlocking(const Arguments& args) {
  // On windows stdin always blocks
  return True();
}


static Handle<Value> IsStdoutBlocking(const Arguments& args) {
  // On windows stdout always blocks
  return True();
}


// process.binding('stdio').getWindowSize(fd);
// returns [row, col]
static Handle<Value> GetWindowSize (const Arguments& args) {
  HandleScope scope;
  int fd;
  HANDLE handle;
  CONSOLE_SCREEN_BUFFER_INFO info;

  if (!args[0]->IsNumber())
    THROW_BAD_ARGS
  fd = args[0]->IntegerValue();
  handle = (HANDLE)_get_osfhandle(fd);

  if (!GetConsoleScreenBufferInfo(handle, &info))
      return ThrowException(ErrnoException(GetLastError(), "GetConsoleScreenBufferInfo"));

  Local<Array> ret = Array::New(2);
  ret->Set(0, Integer::New(static_cast<int>(info.dwSize.Y)));
  ret->Set(1, Integer::New(static_cast<int>(info.dwSize.X)));

  return scope.Close(ret);
}


void Stdio::Initialize(v8::Handle<v8::Object> target) {
  target->Set(String::NewSymbol("stdoutFD"), Integer::New(STDOUT_FILENO));
  target->Set(String::NewSymbol("stderrFD"), Integer::New(STDERR_FILENO));
  target->Set(String::NewSymbol("stdinFD"), Integer::New(STDIN_FILENO));

  NODE_SET_METHOD(target, "writeError", WriteError);
  NODE_SET_METHOD(target, "isatty", IsATTY);
  NODE_SET_METHOD(target, "isStdoutBlocking", IsStdoutBlocking);
  NODE_SET_METHOD(target, "isStdinBlocking", IsStdinBlocking);
  NODE_SET_METHOD(target, "setRawMode", SetRawMode);
  NODE_SET_METHOD(target, "openStdin", OpenStdin);
  NODE_SET_METHOD(target, "getWindowSize", GetWindowSize);
}


}  // namespace node

NODE_MODULE(node_stdio, node::Stdio::Initialize);
