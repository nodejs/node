// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

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

#define KEY(scancode, name) \
    scancodes[scancode] = name;
#define MAX_KEY VK_OEM_PERIOD

static const char* scancodes[MAX_KEY + 1] = {0};

static Persistent<String> name_symbol;
static Persistent<String> shift_symbol;
static Persistent<String> ctrl_symbol;
static Persistent<String> meta_symbol;


static void init_scancode_table() {
  KEY(VK_CANCEL, "break")
  KEY(VK_BACK, "backspace")
  KEY(VK_TAB, "tab")
  KEY(VK_CLEAR, "clear")
  KEY(VK_RETURN, "enter")
  KEY(VK_PAUSE, "pause")
  KEY(VK_ESCAPE, "escape")
  KEY(VK_SPACE, "space")
  KEY(VK_PRIOR, "pageup")
  KEY(VK_NEXT, "pagedown")
  KEY(VK_END, "end")
  KEY(VK_HOME, "home")
  KEY(VK_LEFT, "left")
  KEY(VK_UP, "up")
  KEY(VK_RIGHT, "right")
  KEY(VK_DOWN, "down")
  KEY(VK_SELECT, "select")
  KEY(VK_PRINT, "print")
  KEY(VK_EXECUTE, "execute")
  KEY(VK_SNAPSHOT, "printscreen")
  KEY(VK_INSERT, "insert")
  KEY(VK_DELETE, "delete")
  KEY(VK_HELP, "help")
  KEY(VK_LWIN, "lwin")
  KEY(VK_RWIN, "rwin")
  KEY(VK_APPS, "apps")
  KEY(VK_SLEEP, "sleep")
  KEY(VK_NUMPAD0, "numpad0")
  KEY(VK_NUMPAD1, "numpad1")
  KEY(VK_NUMPAD2, "numpad2")
  KEY(VK_NUMPAD3, "numpad3")
  KEY(VK_NUMPAD4, "numpad4")
  KEY(VK_NUMPAD5, "numpad5")
  KEY(VK_NUMPAD6, "numpad6")
  KEY(VK_NUMPAD7, "numpad7")
  KEY(VK_NUMPAD8, "numpad8")
  KEY(VK_NUMPAD9, "numpad9")
  KEY(VK_MULTIPLY, "numpad*")
  KEY(VK_ADD, "numpad+")
  KEY(VK_SEPARATOR, "numpad,")
  KEY(VK_SUBTRACT, "numpad-")
  KEY(VK_DECIMAL, "numpad.")
  KEY(VK_DIVIDE, "numpad/")
  KEY(VK_F1, "f1")
  KEY(VK_F2, "f2")
  KEY(VK_F3, "f3")
  KEY(VK_F4, "f4")
  KEY(VK_F5, "f5")
  KEY(VK_F6, "f6")
  KEY(VK_F7, "f7")
  KEY(VK_F8, "f8")
  KEY(VK_F9, "f9")
  KEY(VK_F10, "f10")
  KEY(VK_F11, "f11")
  KEY(VK_F12, "f12")
  KEY(VK_F13, "f13")
  KEY(VK_F14, "f14")
  KEY(VK_F15, "f15")
  KEY(VK_F16, "f16")
  KEY(VK_F17, "f17")
  KEY(VK_F18, "f18")
  KEY(VK_F19, "f19")
  KEY(VK_F20, "f20")
  KEY(VK_F21, "f21")
  KEY(VK_F22, "f22")
  KEY(VK_F23, "f23")
  KEY(VK_F24, "f24")
  KEY(VK_OEM_PLUS, "+")
  KEY(VK_OEM_MINUS, "-")
  KEY(VK_OEM_COMMA, ",")
  KEY(VK_OEM_PERIOD, ".")

  // Letter keys have the ascii code of their uppercase equivalent as a scan code
  for (int i = 0; i < 26; i++) {
    char *name = new char[2];
    name[0] = 'a' + i;
    name[1] = '\0';
    KEY('A' + i, name)
  }

  // Number keys have their ascii code as scan code
  for (int i = '0'; i <= '9'; i++) {
    char *name = new char[2];
    name[0] = i;
    name[1] = '\0';
    KEY(i, name)
  }
}


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


static Handle<Value> WriteTTY(const Arguments& args) {
  HandleScope scope;
  int fd, len;
  DWORD written;
  HANDLE handle;

  if (!args[0]->IsNumber())
    THROW_BAD_ARGS

  fd = args[0]->IntegerValue();
  handle = (HANDLE)_get_osfhandle(fd);

  Handle<String> data = args[1]->ToString();
  String::Value buf(data);
  len = data->Length();

  if (!WriteConsoleW(handle, reinterpret_cast<void*>(*buf), len, &written, NULL))
    return ThrowException(ErrnoException(GetLastError(), "WriteConsole"));

  return scope.Close(Integer::New(written));
}


static Handle<Value> CloseTTY(const Arguments& args) {
  HandleScope scope;

  int fd = args[0]->IntegerValue();
  if (close(fd) < 0)
    return ThrowException(ErrnoException(errno, "close"));

  return Undefined();
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


/* moveCursor(fd, dx, dy) */
/* cursorTo(fd, x, y) */
template<bool relative>
static Handle<Value> SetCursor(const Arguments& args) {
  HandleScope scope;
  int fd;
  COORD size, pos;
  HANDLE handle;
  CONSOLE_SCREEN_BUFFER_INFO info;

  if (!args[0]->IsNumber())
    THROW_BAD_ARGS
  fd = args[0]->IntegerValue();
  handle = (HANDLE)_get_osfhandle(fd);

  if (!GetConsoleScreenBufferInfo(handle, &info))
    return ThrowException(ErrnoException(GetLastError(), "GetConsoleScreenBufferInfo"));

  pos = info.dwCursorPosition;
  if (relative) {
    if (args[1]->IsNumber())
      pos.X += static_cast<short>(args[1]->Int32Value());
    if (args[2]->IsNumber())
      pos.Y += static_cast<short>(args[2]->Int32Value());
  } else {
    if (args[1]->IsNumber())
      pos.X = static_cast<short>(args[1]->Int32Value());
    if (args[2]->IsNumber())
      pos.Y = static_cast<short>(args[2]->Int32Value());
  }

  size = info.dwSize;
  if (pos.X >= size.X) pos.X = size.X - 1;
  if (pos.X < 0) pos.X = 0;
  if (pos.Y >= size.Y) pos.Y = size.Y - 1;
  if (pos.Y < 0) pos.Y = 0;

  if (!SetConsoleCursorPosition(handle, pos))
    return ThrowException(ErrnoException(GetLastError(), "SetConsoleCursorPosition"));

  return Undefined();
}


/*
 * ClearLine(fd, direction)
 * direction:
 *   -1: from cursor leftward
 *    0: entire line
 *    1: from cursor to right
 */
static Handle<Value> ClearLine(const Arguments& args) {
  HandleScope scope;
  int fd, dir;
  short x1, x2, count;
  WCHAR *buf;
  COORD pos;
  HANDLE handle;
  CONSOLE_SCREEN_BUFFER_INFO info;
  DWORD res, written, mode, oldmode;

  if (!args[0]->IsNumber())
    THROW_BAD_ARGS
  fd = args[0]->IntegerValue();
  handle = (HANDLE)_get_osfhandle(fd);

  if (args[1]->IsNumber())
    dir = args[1]->IntegerValue();

  if (!GetConsoleScreenBufferInfo(handle, &info))
    return ThrowException(ErrnoException(GetLastError(), "GetConsoleScreenBufferInfo"));

  x1 = dir <= 0 ? 0 : info.dwCursorPosition.X;
  x2 = dir >= 0 ? info.dwSize.X - 1: info.dwCursorPosition.X;
  count = x2 - x1 + 1;

  if (x1 != info.dwCursorPosition.X) {
    pos.Y = info.dwCursorPosition.Y;
    pos.X = x1;
    if (!SetConsoleCursorPosition(handle, pos))
      return ThrowException(ErrnoException(GetLastError(), "SetConsoleCursorPosition"));
  }

  if (!GetConsoleMode(handle, &oldmode))
    return ThrowException(ErrnoException(GetLastError(), "GetConsoleMode"));

  // Disable wrapping at eol because otherwise windows scrolls the console
  // when clearing the last line of the console
  mode = oldmode & ~ENABLE_WRAP_AT_EOL_OUTPUT;
  if (!SetConsoleMode(handle, mode))
    return ThrowException(ErrnoException(GetLastError(), "SetConsoleMode"));

  buf = new WCHAR[count];
  for (short i = 0; i < count; i++) {
    buf[i] = L' ';
  }

  res = WriteConsoleW(handle, buf, count, &written, NULL);

  delete[] buf;

  if (!res)
    return ThrowException(ErrnoException(GetLastError(), "WriteConsole"));

  if (!SetConsoleCursorPosition(handle, info.dwCursorPosition))
    return ThrowException(ErrnoException(GetLastError(), "SetConsoleCursorPosition"));

  if (!SetConsoleMode(handle, oldmode))
    return ThrowException(ErrnoException(GetLastError(), "SetConsoleMode"));

  return Undefined();
}


/* TTY watcher data */
HANDLE tty_handle;
HANDLE tty_wait_handle;
void *tty_error_callback;
void *tty_keypress_callback;
void *tty_resize_callback;
static bool tty_watcher_initialized = false;
static bool tty_watcher_active = false;
static uv_async_t tty_avail_notifier;


static void CALLBACK tty_want_poll(void *context, BOOLEAN didTimeout) {
  assert(!didTimeout);
  uv_async_send(&tty_avail_notifier);
}


static void tty_watcher_arm() {
  // Register a new wait handle before dropping the old one, because
  // otherwise windows might destroy and recreate the wait thread.
  // MSDN promises that thread pool threads are kept alive when they're idle,
  // but apparently this does not apply to wait threads. Sigh.

  HANDLE old_wait_handle = tty_wait_handle;
  tty_wait_handle = NULL;

  assert(tty_watcher_active);

  if (!RegisterWaitForSingleObject(&tty_wait_handle, tty_handle, tty_want_poll, NULL,
      INFINITE, WT_EXECUTEINWAITTHREAD | WT_EXECUTEONLYONCE)) {
    ThrowException(ErrnoException(GetLastError(), "RegisterWaitForSingleObject"));
  }

  if (old_wait_handle != NULL) {
    if (!UnregisterWait(old_wait_handle) && GetLastError() != ERROR_IO_PENDING)
      ThrowException(ErrnoException(GetLastError(), "UnregisterWait"));
  }
}


static void tty_watcher_disarm() {
  DWORD result;
  if (tty_wait_handle != NULL) {
    result = UnregisterWait(tty_wait_handle);
    tty_wait_handle = NULL;
    if (!result && GetLastError() != ERROR_IO_PENDING)
      ThrowException(ErrnoException(GetLastError(), "UnregisterWait"));
  }
}


static void tty_watcher_start() {
  assert(tty_watcher_initialized);
  if (!tty_watcher_active) {
    tty_watcher_active = true;
    uv_ref();
    tty_watcher_arm();
  }
}


static void tty_watcher_stop() {
  if (tty_watcher_active) {
    tty_watcher_active = false;
    uv_unref();
    tty_watcher_disarm();
  }
}


static inline void tty_emit_error(Handle<Value> err) {
  HandleScope scope;
  Handle<Object> global = v8::Context::GetCurrent()->Global();
  Handle<Function> *handler = cb_unwrap(tty_error_callback);
  Handle<Value> argv[1] = { err };
  (*handler)->Call(global, 1, argv);
}


static void tty_poll(uv_handle_t* handle, int status) {
  assert((uv_async_t*) handle == &tty_avail_notifier);
  assert(status == 0);

  HandleScope scope;
  TryCatch try_catch;
  Handle<Object> global = v8::Context::GetCurrent()->Global();
  Handle<Function> *callback;
  INPUT_RECORD input;
  KEY_EVENT_RECORD k;
  const char *keyName;
  DWORD i, j, numev, read;
  Handle<Value> argv[2];
  Handle<Object> key;

  if (!GetNumberOfConsoleInputEvents(tty_handle, &numev)) {
    tty_emit_error(ErrnoException(GetLastError(),
        "GetNumberOfConsoleInputEvents"));
    numev = 0;
  }

  for (i = numev; i > 0 &&
      tty_watcher_active; i--) {
    if (!ReadConsoleInputW(tty_handle, &input, 1, &read)) {
      tty_emit_error(ErrnoException(GetLastError(), "ReadConsoleInputW"));
      break;
    }

    switch (input.EventType) {
      case KEY_EVENT:
        // Skip if no callback set
        if (!tty_keypress_callback)
          break;

        k = input.Event.KeyEvent;

        // Ignore keyup
        if (!k.bKeyDown)
          break;

        // Try to find a symbolic name for the key
        keyName = (k.wVirtualKeyCode <= MAX_KEY)
            ? scancodes[k.wVirtualKeyCode]
            : 0;

        // The key must have a symbolic name or a char or both
        if (k.uChar.UnicodeChar == 0 && keyName == 0)
          break;

        // Set the event name and character
        argv[0] = k.uChar.UnicodeChar
            ? String::New(reinterpret_cast<uint16_t*>(&k.uChar.UnicodeChar), 1)
            : Undefined();

        // Set the key info, if any
        if (keyName) {
          key = Object::New();
          key->Set(name_symbol, String::New(keyName));
          key->Set(shift_symbol, Boolean::New(k.dwControlKeyState &
              SHIFT_PRESSED));
          key->Set(ctrl_symbol, Boolean::New(k.dwControlKeyState &
              (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)));
          key->Set(meta_symbol, Boolean::New(k.dwControlKeyState &
              (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)));
          argv[1] = key;
        } else {
          argv[1] = Undefined();
        }

        callback = cb_unwrap(tty_keypress_callback);
        j = k.wRepeatCount;
        do {
          (*callback)->Call(global, 2, argv);
        } while (--j > 0 && tty_watcher_active);
        break;

      case WINDOW_BUFFER_SIZE_EVENT:
        if (!tty_resize_callback)
          break;
        callback = cb_unwrap(tty_resize_callback);
        (*callback)->Call(global, 0, argv);
        break;
    }
  }

  // Rearm the watcher
  tty_watcher_arm();

  // Emit fatal errors and unhandled error events
  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
}


/* StartTTYWatcher(fd, onError, onKeypress, onResize) */
static Handle<Value> InitTTYWatcher(const Arguments& args) {
  HandleScope scope;

  if (tty_watcher_initialized)
    THROW_ERROR("TTY watcher already initialized")

  if (!args[0]->IsNumber())
    THROW_BAD_ARGS;
  tty_handle = (HANDLE)_get_osfhandle(args[0]->IntegerValue());

  if (!args[1]->IsFunction())
    THROW_BAD_ARGS;
  tty_error_callback = cb_persist(args[1]);

  tty_keypress_callback = args[2]->IsFunction()
      ? cb_persist(args[2])
      : NULL;

  tty_resize_callback = args[3]->IsFunction()
      ? cb_persist(args[3])
      : NULL;

  tty_watcher_initialized = true;
  tty_wait_handle = NULL;

  return Undefined();
}


static Handle<Value> DestroyTTYWatcher(const Arguments& args) {
  if (!tty_watcher_initialized)
    THROW_ERROR("TTY watcher not initialized")

  tty_watcher_stop();

  if (tty_error_callback != NULL)
    cb_destroy(cb_unwrap(tty_error_callback));
  if (tty_keypress_callback != NULL)
    cb_destroy(cb_unwrap(tty_keypress_callback));
  if (tty_resize_callback != NULL)
    cb_destroy(cb_unwrap(tty_resize_callback));

  tty_watcher_initialized = false;

  return Undefined();
}


static Handle<Value> StartTTYWatcher(const Arguments& args) {
  if (!tty_watcher_initialized)
    THROW_ERROR("TTY watcher not initialized")

  tty_watcher_start();
  return Undefined();
}


static Handle<Value> StopTTYWatcher(const Arguments& args) {
  if (!tty_watcher_initialized)
    THROW_ERROR("TTY watcher not initialized")

  tty_watcher_stop();
  return Undefined();
}


void Stdio::Initialize(v8::Handle<v8::Object> target) {
  init_scancode_table();
  
  uv_async_init(&tty_avail_notifier, tty_poll, NULL, NULL);
  uv_unref();

  name_symbol = NODE_PSYMBOL("name");
  shift_symbol = NODE_PSYMBOL("shift");
  ctrl_symbol = NODE_PSYMBOL("ctrl");
  meta_symbol = NODE_PSYMBOL("meta");

  target->Set(String::NewSymbol("stdoutFD"), Integer::New(STDOUT_FILENO));
  target->Set(String::NewSymbol("stderrFD"), Integer::New(STDERR_FILENO));
  target->Set(String::NewSymbol("stdinFD"), Integer::New(STDIN_FILENO));

  NODE_SET_METHOD(target, "writeError", WriteError);
  NODE_SET_METHOD(target, "isatty", IsATTY);
  NODE_SET_METHOD(target, "isStdoutBlocking", IsStdoutBlocking);
  NODE_SET_METHOD(target, "isStdinBlocking", IsStdinBlocking);
  NODE_SET_METHOD(target, "setRawMode", SetRawMode);
  NODE_SET_METHOD(target, "openStdin", OpenStdin);
  NODE_SET_METHOD(target, "writeTTY", WriteTTY);
  NODE_SET_METHOD(target, "closeTTY", CloseTTY);
  NODE_SET_METHOD(target, "moveCursor", SetCursor<true>);
  NODE_SET_METHOD(target, "cursorTo", SetCursor<false>);
  NODE_SET_METHOD(target, "clearLine", ClearLine);
  NODE_SET_METHOD(target, "getWindowSize", GetWindowSize);
  NODE_SET_METHOD(target, "initTTYWatcher", InitTTYWatcher);
  NODE_SET_METHOD(target, "destroyTTYWatcher", DestroyTTYWatcher);
  NODE_SET_METHOD(target, "startTTYWatcher", StartTTYWatcher);
  NODE_SET_METHOD(target, "stopTTYWatcher", StopTTYWatcher);
}


}  // namespace node

NODE_MODULE(node_stdio, node::Stdio::Initialize);
