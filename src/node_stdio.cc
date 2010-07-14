#include <node_stdio.h>
#include <node_events.h>

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <termios.h>
#include <sys/ioctl.h>

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
  /* output modes - disable post processing */
  raw.c_oflag &= ~(OPOST);
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


// process.binding('stdio').getColumns();
static Handle<Value> GetColumns (const Arguments& args) {
  HandleScope scope;

  struct winsize ws;

  if (ioctl(1, TIOCGWINSZ, &ws) == -1) {
    return scope.Close(Integer::New(80));
  }

  return scope.Close(Integer::NewFromUnsigned(ws.ws_col));
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


static bool IsBlocking(int fd) {
  if (isatty(fd)) return false;
  struct stat s;
  if (fstat(fd, &s)) {
    perror("fstat");
    return true;
  }
  if (s.st_mode & S_IFSOCK == S_IFSOCK) return false;
  if (s.st_mode & S_IFIFO == S_IFIFO) return false;
  return true;
}


static Handle<Value> IsStdinBlocking(const Arguments& arg) {
  return IsBlocking(STDIN_FILENO) ? True() : False();
}


static Handle<Value> IsStdoutBlocking(const Arguments& args) {
  return IsBlocking(STDOUT_FILENO) ? True() : False();
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
    int r = fcntl(STDOUT_FILENO, F_SETFL, stdout_flags | O_NONBLOCK);
  }

  target->Set(String::NewSymbol("stdoutFD"), Integer::New(STDOUT_FILENO));
  target->Set(String::NewSymbol("stderrFD"), Integer::New(STDERR_FILENO));
  target->Set(String::NewSymbol("stdinFD"), Integer::New(STDIN_FILENO));

  NODE_SET_METHOD(target, "writeError", WriteError);
  NODE_SET_METHOD(target, "openStdin", OpenStdin);
  NODE_SET_METHOD(target, "isStdoutBlocking", IsStdoutBlocking);
  NODE_SET_METHOD(target, "isStdinBlocking", IsStdinBlocking);
  NODE_SET_METHOD(target, "setRawMode", SetRawMode);
  NODE_SET_METHOD(target, "getColumns", GetColumns);

  struct sigaction sa = {0};
  sa.sa_handler = HandleSIGCONT;
  sigaction(SIGCONT, &sa, NULL);
}


}  // namespace node

NODE_MODULE(node_stdio, node::Stdio::Initialize);
