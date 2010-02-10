#include <node_stdio.h>
#include <node_events.h>
#include <coupling.h>

#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

using namespace v8;
using namespace node;

static Persistent<Object> stdio;
static Persistent<Function> emit;

static struct coupling *stdin_coupling = NULL;
static struct coupling *stdout_coupling = NULL;

static int stdin_fd = -1;
static int stdout_fd = -1;

static evcom_reader in;
static evcom_writer out;

static enum encoding stdin_encoding;

static void
EmitInput (Local<Value> input)
{
  HandleScope scope;

  Local<Value> argv[2] = { String::NewSymbol("data"), input };

  emit->Call(stdio, 2, argv);
}

static void
EmitClose (void)
{
  HandleScope scope;

  Local<Value> argv[1] = { String::NewSymbol("close") };

  emit->Call(stdio, 1, argv);
}


static inline Local<Value> errno_exception(int errorno) {
  Local<Value> e = Exception::Error(String::NewSymbol(strerror(errorno)));
  Local<Object> obj = e->ToObject();
  obj->Set(String::NewSymbol("errno"), Integer::New(errorno));
  return e;
}


/* STDERR IS ALWAY SYNC */
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

static Handle<Value>
Write (const Arguments& args)
{
  HandleScope scope;

  if (args.Length() == 0) {
    return ThrowException(Exception::Error(String::New("Bad argument")));
  }

  enum encoding enc = UTF8;
  if (args.Length() > 1) enc = ParseEncoding(args[1], UTF8);

  ssize_t len = DecodeBytes(args[0], enc);

  if (len < 0) {
    Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
    return ThrowException(exception);
  }

  char buf[len];
  ssize_t written = DecodeWrite(buf, len, args[0], enc);
  
  assert(written == len);

  evcom_writer_write(&out, buf, len);

  return Undefined();
}

static void
detach_in (evcom_reader *r)
{
  assert(r == &in);
  HandleScope scope;

  EmitClose();

  evcom_reader_detach(&in);

  if (stdin_coupling) {
    coupling_destroy(stdin_coupling);
    stdin_coupling = NULL;
  }

  stdin_fd = -1;
}

static void
detach_out (evcom_writer* w)
{
  assert(w == &out);

  evcom_writer_detach(&out);
  if (stdout_coupling) {
    coupling_destroy(stdout_coupling);
    stdout_coupling = NULL;
  }
  stdout_fd = -1;
}

static void
on_read (evcom_reader *r, const void *buf, size_t len)
{
  assert(r == &in);
  HandleScope scope;

  if (!len) {
    return;
  }

  Local<Value> data = Encode(buf, len, stdin_encoding);

  EmitInput(data);
}

static inline int
set_nonblock (int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) return -1;

  int r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  if (r == -1) return -1;

  return 0;
}

static Handle<Value>
Open (const Arguments& args)
{
  HandleScope scope;

  if (stdin_fd >= 0) {
    return ThrowException(Exception::Error(String::New("stdin already open")));
  }

  stdin_encoding = UTF8;
  if (args.Length() > 0) {
    stdin_encoding = ParseEncoding(args[0]);
  }

  if (isatty(STDIN_FILENO)) {
    // XXX selecting on tty fds wont work in windows.
    // Must ALWAYS make a coupling on shitty platforms.
    stdin_fd = STDIN_FILENO;
  } else {
    stdin_coupling = coupling_new_pull(STDIN_FILENO);
    stdin_fd = coupling_nonblocking_fd(stdin_coupling);
  }
  set_nonblock(stdin_fd);

  evcom_reader_init(&in);

  in.on_read = on_read;
  in.on_close = detach_in;

  evcom_reader_set(&in, stdin_fd);
  evcom_reader_attach(EV_DEFAULT_ &in);

  return Undefined();
}

static Handle<Value>
Close (const Arguments& args)
{
  HandleScope scope;

  assert(stdio == args.Holder());

  if (stdin_fd < 0) {
    return ThrowException(Exception::Error(String::New("stdin not open")));
  }

  evcom_reader_close(&in);

  return Undefined();
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

void
Stdio::Initialize (v8::Handle<v8::Object> target)
{
  HandleScope scope;

  Local<Object> stdio_local =
    EventEmitter::constructor_template->GetFunction()->NewInstance(0, NULL);

  stdio = Persistent<Object>::New(stdio_local);

  NODE_SET_METHOD(stdio, "open", Open);
  NODE_SET_METHOD(stdio, "write", Write);
  NODE_SET_METHOD(stdio, "writeError", WriteError);
  NODE_SET_METHOD(stdio, "close", Close);

  target->Set(String::NewSymbol("stdio"), stdio);

  Local<Value> emit_v = stdio->Get(String::NewSymbol("emit"));
  assert(emit_v->IsFunction());
  Local<Function> emit_f = Local<Function>::Cast(emit_v);
  emit = Persistent<Function>::New(emit_f);

  if (isatty(STDOUT_FILENO)) {
    // XXX selecting on tty fds wont work in windows.
    // Must ALWAYS make a coupling on shitty platforms.
    stdout_fd = STDOUT_FILENO;
  } else {
    stdout_coupling = coupling_new_push(STDOUT_FILENO);
    stdout_fd = coupling_nonblocking_fd(stdout_coupling);
  }
  set_nonblock(stdout_fd);

  evcom_writer_init(&out);
  out.on_close = detach_out;
  evcom_writer_set(&out, stdout_fd);
  evcom_writer_attach(EV_DEFAULT_ &out);
}
