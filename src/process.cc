#include "node.h"
#include "process.h"

#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

using namespace v8;
using namespace node;

#define PID_SYMBOL String::NewSymbol("pid")

Persistent<FunctionTemplate> Process::constructor_template;

void
Process::Initialize (Handle<Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(Process::New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->Inherit(EventEmitter::constructor_template);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "spawn", Process::Spawn);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "write", Process::Write);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "close", Process::Close);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "kill", Process::Kill);

  target->Set(String::NewSymbol("Process"), constructor_template->GetFunction());
}

Handle<Value>
Process::New (const Arguments& args)
{
  HandleScope scope;

  Process *p = new Process();
  p->Wrap(args.Holder());

  return args.This();
}

Handle<Value>
Process::Spawn (const Arguments& args)
{
  if (args.Length() == 0 || !args[0]->IsString()) {
    return ThrowException(String::New("Bad argument."));
  }

  HandleScope scope;
  Process *process = ObjectWrap::Unwrap<Process>(args.Holder());

  String::Utf8Value command(args[0]->ToString());

  int r = process->Spawn(*command);
  if (r != 0) {
    return ThrowException(String::New("Error spawning"));
  }

  process->handle_->Set(PID_SYMBOL, Integer::New(process->pid_));

  return Undefined();
}

Handle<Value>
Process::Write (const Arguments& args)
{
  HandleScope scope;
  Process *process = ObjectWrap::Unwrap<Process>(args.Holder());
  assert(process);

  ssize_t len;

  Local<String> string;
  Local<Array> array;

  if (args[0]->IsArray()) {
    array = Local<Array>::Cast(args[0]);
    len = array->Length();
  } else {
    string = args[0]->ToString();
    len = string->Utf8Length();
  }

  char buf[len];

  if (args[0]->IsArray()) {
    for (ssize_t index = 0; index < len; index++) {
      Local<Value> int_value = array->Get(Integer::New(index));
      buf[index] = int_value->IntegerValue();
    }
  } else {
    switch (ParseEncoding(args[1])) {
      case RAW:
      case ASCII:
        string->WriteAscii(buf, 0, len);
        break;

      case UTF8:
        string->WriteUtf8(buf, len);
        break;

      default:
        return ThrowException(String::New("Unknown encoding."));
    }
  }

  return process->Write(buf, len) == 0 ? True() : False();
}

Handle<Value>
Process::Kill (const Arguments& args)
{
  HandleScope scope;
  Process *process = ObjectWrap::Unwrap<Process>(args.Holder());
  assert(process);

  int sig = SIGTERM;
  if (args[0]->IsInt32()) sig = args[0]->Int32Value();

  if (process->Kill(sig) != 0) {
    return ThrowException(String::New("Process already dead"));
  }

  return Undefined();
}

Handle<Value>
Process::Close (const Arguments& args)
{
  HandleScope scope;
  Process *process = ObjectWrap::Unwrap<Process>(args.Holder());
  assert(process);
  return process->Close() == 0 ? True() : False();
}

void
Process::reader_closed (evcom_reader *r)
{
  Process *process = static_cast<Process*> (r->data);
  if (r == &process->stdout_reader_) {
    process->stdout_fd_ = -1;
  } else {
    assert(r == &process->stderr_reader_);
    process->stderr_fd_ = -1;
  }
  evcom_reader_detach(r);
  process->MaybeShutdown();
}

void
Process::stdin_closed (evcom_writer *w)
{
  Process *process = static_cast<Process*> (w->data);
  assert(w == &process->stdin_writer_);
  process->stdin_fd_ = -1;
  evcom_writer_detach(w);
  process->MaybeShutdown();
}

void
Process::on_read (evcom_reader *r, const void *buf, size_t len)
{
  Process *process = static_cast<Process*> (r->data);
  HandleScope scope;

  bool isSTDOUT = (r == &process->stdout_reader_);
  Local<Value> argv[1];

  enum encoding encoding = isSTDOUT ? process->stdout_encoding_ : process->stderr_encoding_;

  if (len == 0) {
    argv[0] = Local<Value>::New(Null());

  } else if (encoding == RAW) {
    // raw encoding
    Local<Array> array = Array::New(len);
    for (size_t i = 0; i < len; i++) {
      unsigned char val = static_cast<const unsigned char*>(buf)[i];
      array->Set(Integer::New(i), Integer::New(val));
    }
    argv[0] = array;

  } else {
    // utf8 or ascii encoding
    argv[0] = String::New((const char*)buf, len);
  }

  process->Emit(isSTDOUT ? "output" : "error", 1, argv);
  process->MaybeShutdown();
}

Process::Process ()
  : EventEmitter()
{
  evcom_reader_init(&stdout_reader_);
  stdout_reader_.data     = this;
  stdout_reader_.on_read  = on_read;
  stdout_reader_.on_close = reader_closed;

  evcom_reader_init(&stderr_reader_);
  stderr_reader_.data     = this;
  stderr_reader_.on_read  = on_read;
  stderr_reader_.on_close = reader_closed;

  evcom_writer_init(&stdin_writer_);
  stdin_writer_.data      = this;
  stdin_writer_.on_close  = stdin_closed;

  ev_init(&child_watcher_, Process::OnCHLD);
  child_watcher_.data = this;

  stdout_fd_ = -1;
  stderr_fd_ = -1;
  stdin_fd_ = -1;

  stdout_encoding_ = UTF8;
  stderr_encoding_ = UTF8;

  got_chld_ = false;
  exit_code_ = 0;

  pid_ = 0;
}

Process::~Process ()
{
  Shutdown();
}

void
Process::Shutdown ()
{
  if (stdin_fd_ >= 0) {
    evcom_writer_close(&stdin_writer_);
  }

  if (stdin_fd_  >= 0) close(stdin_fd_);
  if (stdout_fd_ >= 0) close(stdout_fd_);
  if (stderr_fd_ >= 0) close(stderr_fd_);

  stdin_fd_ = -1;
  stdout_fd_ = -1;
  stderr_fd_ = -1;

  evcom_writer_detach(&stdin_writer_);
  evcom_reader_detach(&stdout_reader_);
  evcom_reader_detach(&stderr_reader_);

  ev_child_stop(EV_DEFAULT_UC_ &child_watcher_);

  /* XXX Kill the PID? */
  pid_ = 0;
}

static inline int
SetNonBlocking (int fd)
{
  int flags = fcntl(fd, F_GETFL, 0);
  int r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
  if (r != 0) {
    perror("SetNonBlocking()");
  }
  return r;
}

int
Process::Spawn (const char *command)
{
  assert(pid_ == 0);
  assert(stdout_fd_ == -1);
  assert(stderr_fd_ == -1);
  assert(stdin_fd_ == -1);

  int stdout_pipe[2], stdin_pipe[2], stderr_pipe[2];

  /* An implementation of popen(), basically */
  if (pipe(stdout_pipe) < 0) {
    perror("pipe()");
    return -1;
  }

  if (pipe(stderr_pipe) < 0) {
    perror("pipe()");
    return -2;
  }

  if (pipe(stdin_pipe) < 0) {
    perror("pipe()");
    return -3;
  }

  switch (pid_ = vfork()) {
    case -1: // Error.
      Shutdown();
      return -4;

    case 0: // Child.
      close(stdout_pipe[0]); // close read end
      dup2(stdout_pipe[1], STDOUT_FILENO);

      close(stderr_pipe[0]); // close read end
      dup2(stderr_pipe[1], STDERR_FILENO);

      close(stdin_pipe[1]); // close write end
      dup2(stdin_pipe[0],  STDIN_FILENO);

      execl("/bin/sh", "sh", "-c", command, (char *)NULL);
      _exit(127);
  }

  // Parent.

  ev_child_set(&child_watcher_, pid_, 0);
  ev_child_start(EV_DEFAULT_UC_ &child_watcher_);

  close(stdout_pipe[1]);
  stdout_fd_ = stdout_pipe[0];
  SetNonBlocking(stdout_fd_);

  close(stderr_pipe[1]);
  stderr_fd_ = stderr_pipe[0];
  SetNonBlocking(stderr_fd_);

  close(stdin_pipe[0]);
  stdin_fd_ = stdin_pipe[1];
  SetNonBlocking(stdin_fd_);

  evcom_reader_set(&stdout_reader_, stdout_fd_);
  evcom_reader_attach(EV_DEFAULT_UC_ &stdout_reader_);

  evcom_reader_set(&stderr_reader_, stderr_fd_);
  evcom_reader_attach(EV_DEFAULT_UC_ &stderr_reader_);

  evcom_writer_set(&stdin_writer_, stdin_fd_);
  evcom_writer_attach(EV_DEFAULT_UC_ &stdin_writer_);

  Attach();

  return 0;
}

void
Process::OnCHLD (EV_P_ ev_child *watcher, int revents)
{
  ev_child_stop(EV_A_ watcher);
  Process *process = static_cast<Process*>(watcher->data);

  assert(revents == EV_CHILD);
  assert(process->pid_ == watcher->rpid);
  assert(&process->child_watcher_ == watcher);

  process->got_chld_ = true;
  process->exit_code_ = watcher->rstatus;

  if (process->stdin_fd_  >= 0) evcom_writer_close(&process->stdin_writer_);

  process->MaybeShutdown();
}

int
Process::Write (const char *str, size_t len)
{
  if (stdin_fd_ < 0 || got_chld_) return -1;
  evcom_writer_write(&stdin_writer_, str, len);
  return 0;
}

int
Process::Close (void)
{
  if (stdin_fd_ < 0 || got_chld_) return -1;
  evcom_writer_close(EV_DEFAULT_UC_ &stdin_writer_);
  return 0;
}

int
Process::Kill (int sig)
{
  if (got_chld_ || pid_ == 0) return -1;
  return kill(pid_, sig);
}

void
Process::MaybeShutdown (void)
{
  if (stdout_fd_ < 0 && stderr_fd_ < 0 && got_chld_) {
    HandleScope scope;
    Handle<Value> argv[1] = { Integer::New(exit_code_) };
    Emit("exit", 1, argv);
    Shutdown();
    Detach();
  }
}
