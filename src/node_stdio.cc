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

#include <node_stdio.h>
#include <node_events.h>

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#if defined(__APPLE__) || defined(__OpenBSD__)
# include <util.h>
#elif __FreeBSD__
# include <libutil.h>
#elif defined(__sun)
# include <stropts.h> // for openpty ioctls
#else
# include <pty.h>
#endif

#include <termios.h>
#include <sys/ioctl.h>
#include <stdlib.h>

using namespace v8;
namespace node {


static int stdout_flags = -1;
static int stdin_flags = -1;


static struct termios orig_termios; /* in order to restore at exit */
static int rawmode = 0; /* for atexit() function to check if restore is needed*/


static int EnableRawMode(int fd) {
  struct termios raw;

  if (rawmode) return 0;

  //if (!isatty(fd)) goto fatal;
  if (tcgetattr(fd, &orig_termios) == -1) goto fatal;

  raw = orig_termios;  /* modify the original mode */
  /* input modes: no break, no CR to NL, no parity check, no strip char,
   * no start/stop output control. */
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  /* output modes */
  raw.c_oflag |= (ONLCR);
  /* control modes - set 8 bit chars */
  raw.c_cflag |= (CS8);
  /* local modes - choing off, canonical off, no extended functions,
   * no signal chars (^Z,^C) */
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  /* control chars - set return condition: min number of bytes and timer.
   * We want read to return every single byte, without timeout. */
  raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0; /* 1 byte, no timer */

  /* put terminal in raw mode after flushing */
  if (tcsetattr(fd, TCSAFLUSH, &raw) < 0) goto fatal;
  rawmode = 1;
  return 0;

fatal:
  errno = ENOTTY;
  return -1;
}


void Stdio::DisableRawMode(int fd) {
  /* Don't even check the return value as it's too late. */
  if (rawmode && tcsetattr(fd, TCSAFLUSH, &orig_termios) != -1) {
    rawmode = 0;
  }
}

// process.binding('stdio').setRawMode(true);
static Handle<Value> SetRawMode (const Arguments& args) {
  HandleScope scope;

  if (args[0]->IsFalse()) {
    Stdio::DisableRawMode(STDIN_FILENO);
  } else {
    if (0 != EnableRawMode(STDIN_FILENO)) {
      return ThrowException(ErrnoException(errno, "EnableRawMode"));
    }
  }

  return rawmode ? True() : False();
}


// process.binding('stdio').getWindowSize(fd);
// returns [row, col]
static Handle<Value> GetWindowSize (const Arguments& args) {
  HandleScope scope;

  int fd = args[0]->IntegerValue();

  struct winsize ws;

  if (ioctl(fd, TIOCGWINSZ, &ws) < 0) {
    return ThrowException(ErrnoException(errno, "ioctl"));
  }

  Local<Array> ret = Array::New(2);
  ret->Set(0, Integer::NewFromUnsigned(ws.ws_row));
  ret->Set(1, Integer::NewFromUnsigned(ws.ws_col));

  return scope.Close(ret);
}


// process.binding('stdio').setWindowSize(fd, row, col);
static Handle<Value> SetWindowSize (const Arguments& args) {
  HandleScope scope;

  int fd = args[0]->IntegerValue();
  int row = args[1]->IntegerValue();
  int col = args[2]->IntegerValue();

  struct winsize ws;

  ws.ws_row = row;
  ws.ws_col = col;
  ws.ws_xpixel = 0;
  ws.ws_ypixel = 0;

  if (ioctl(fd, TIOCSWINSZ, &ws) < 0) {
    return ThrowException(ErrnoException(errno, "ioctl"));
  }

  return True();
}


static Handle<Value> IsATTY (const Arguments& args) {
  HandleScope scope;

  int fd = args[0]->IntegerValue();

  int r = isatty(fd);

  return scope.Close(r ? True() : False());
}


/* STDERR IS ALWAY SYNC ALWAYS UTF8 */
static Handle<Value> WriteError (const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1) {
    return Undefined();
  }

  String::Utf8Value msg(args[0]->ToString());

  ssize_t r;
  size_t written = 0;
  while (written < (size_t) msg.length()) {
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

  return True();
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


static bool IsBlocking(int fd) {
  if (isatty(fd)) return false;
  struct stat s;
  if (fstat(fd, &s)) {
    perror("fstat");
    return true;
  }
  return !S_ISSOCK(s.st_mode) && !S_ISFIFO(s.st_mode);
}


static Handle<Value> IsStdinBlocking(const Arguments& arg) {
  return IsBlocking(STDIN_FILENO) ? True() : False();
}


static Handle<Value> IsStdoutBlocking(const Arguments& args) {
  return IsBlocking(STDOUT_FILENO) ? True() : False();
}


static Handle<Value> OpenPTY(const Arguments& args) {
  HandleScope scope;

  int master_fd, slave_fd;

#ifdef __sun

typedef void (*sighandler)(int);
  // TODO move to platform files.
  master_fd = open("/dev/ptmx", O_RDWR | O_NOCTTY);
  sighandler sig_saved = signal(SIGCHLD, SIG_DFL);
  grantpt(master_fd);
  unlockpt(master_fd);
  signal(SIGCHLD, sig_saved);
  char *slave_name = ptsname(master_fd);
  slave_fd = open(slave_name, O_RDWR);
  ioctl(slave_fd, I_PUSH, "ptem");
  ioctl(slave_fd, I_PUSH, "ldterm");
  ioctl(slave_fd, I_PUSH, "ttcompat");

#else

  int r = openpty(&master_fd, &slave_fd, NULL, NULL, NULL);

  if (r == -1) {
    return ThrowException(ErrnoException(errno, "openpty"));
  }
#endif

  Local<Array> a = Array::New(2);

  a->Set(0, Integer::New(master_fd));
  a->Set(1, Integer::New(slave_fd));

  return scope.Close(a);
}


void Stdio::Flush() {
  if (stdin_flags != -1) {
    fcntl(STDIN_FILENO, F_SETFL, stdin_flags & ~O_NONBLOCK);
  }

  if (stdout_flags != -1) {
    fcntl(STDOUT_FILENO, F_SETFL, stdout_flags & ~O_NONBLOCK);
  }

  fflush(stdout);
  fflush(stderr);
}

static void HandleSIGCONT (int signum) {
  if (rawmode) {
    rawmode = 0;
    EnableRawMode(STDIN_FILENO);
  }
}


void Stdio::Initialize(v8::Handle<v8::Object> target) {
  HandleScope scope;

  if (isatty(STDOUT_FILENO)) {
    // XXX selecting on tty fds wont work in windows.
    // Must ALWAYS make a coupling on shitty platforms.
    stdout_flags = fcntl(STDOUT_FILENO, F_GETFL, 0);
    fcntl(STDOUT_FILENO, F_SETFL, stdout_flags | O_NONBLOCK);
  }

  target->Set(String::NewSymbol("stdoutFD"), Integer::New(STDOUT_FILENO));
  target->Set(String::NewSymbol("stderrFD"), Integer::New(STDERR_FILENO));
  target->Set(String::NewSymbol("stdinFD"), Integer::New(STDIN_FILENO));

  NODE_SET_METHOD(target, "writeError", WriteError);
  NODE_SET_METHOD(target, "openStdin", OpenStdin);
  NODE_SET_METHOD(target, "isStdoutBlocking", IsStdoutBlocking);
  NODE_SET_METHOD(target, "isStdinBlocking", IsStdinBlocking);
  NODE_SET_METHOD(target, "setRawMode", SetRawMode);
  NODE_SET_METHOD(target, "getWindowSize", GetWindowSize);
  NODE_SET_METHOD(target, "setWindowSize", SetWindowSize);
  NODE_SET_METHOD(target, "isatty", IsATTY);
  NODE_SET_METHOD(target, "openpty", OpenPTY);

  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = HandleSIGCONT;
  sigaction(SIGCONT, &sa, NULL);
}


}  // namespace node

NODE_MODULE(node_stdio, node::Stdio::Initialize);
