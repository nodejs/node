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

#define ON_ERROR_SYMBOL   String::NewSymbol("onError")
#define ON_OUTPUT_SYMBOL  String::NewSymbol("onOutput")
#define ON_EXIT_SYMBOL    String::NewSymbol("onExit")
#define PID_SYMBOL        String::NewSymbol("pid")

/* defines for the parent side */
#define STDOUT_CLOSED     (stdout_pipe_[0] < 0)
#define STDERR_CLOSED     (stderr_pipe_[0] < 0)
#define STDIN_CLOSED      (stdin_pipe_[1] < 0)


Persistent<FunctionTemplate> Process::constructor_template;

void
Process::Initialize (Handle<Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(Process::New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->Inherit(EventEmitter::constructor_template);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "write", Process::Write);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "close", Process::Close);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "kill", Process::Kill);

  constructor_template->PrototypeTemplate()->SetAccessor(PID_SYMBOL,
                                                         PIDGetter);

  target->Set(String::NewSymbol("Process"), constructor_template->GetFunction());
}

Handle<Value>
Process::New (const Arguments& args)
{
  if (args.Length() == 0) return Undefined();

  HandleScope scope;

  String::Utf8Value command(args[0]->ToString());

  Process *p = new Process(args.Holder());
  ObjectWrap::InformV8ofAllocation(p);

  int r = p->Spawn(*command);
  if (r != 0) {
    return ThrowException(String::New("Error spawning"));
  }

  return args.This();
}

Handle<Value>
Process::PIDGetter (Local<String> _, const AccessorInfo& info)
{
  Process *process = NODE_UNWRAP(Process, info.This());
  assert(process);

  HandleScope scope;

  if (process->pid_ == 0) return Null();

  Local<Integer> pid = Integer::New(process->pid_);
  return scope.Close(pid);
}

Handle<Value>
Process::Write (const Arguments& args)
{
  HandleScope scope;
  Process *process = NODE_UNWRAP(Process, args.Holder());
  assert(process);

  // XXX
  // A lot of improvement can be made here. First of all we're allocating
  // oi_bufs for every send which is clearly inefficent - it should use a
  // memory pool or ring buffer. Of course, expressing binary data as an
  // array of integers is extremely inefficent. This can improved when v8
  // bug 270 (http://code.google.com/p/v8/issues/detail?id=270) has been
  // addressed. 

  oi_buf *buf;
  size_t len;

  if (args[0]->IsString()) {
    enum encoding enc = ParseEncoding(args[1]);
    Local<String> s = args[0]->ToString();
    len = s->Utf8Length();
    buf = node::buf_new(len);
    switch (enc) {
      case RAW:
      case ASCII:
        s->WriteAscii(buf->base, 0, len);
        break;

      case UTF8:
        s->WriteUtf8(buf->base, len);
        break;

      default:
        assert(0 && "unhandled string encoding");
    }

  } else if (args[0]->IsArray()) {
    Handle<Array> array = Handle<Array>::Cast(args[0]);
    len = array->Length();
    buf = node::buf_new(len);
    for (size_t i = 0; i < len; i++) {
      Local<Value> int_value = array->Get(Integer::New(i));
      buf->base[i] = int_value->IntegerValue();
    }

  } else return ThrowException(String::New("Bad argument"));

  return process->Write(buf) == 0 ? True() : False();
}

Handle<Value>
Process::Kill (const Arguments& args)
{
  HandleScope scope;
  Process *process = NODE_UNWRAP(Process, args.Holder());
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
  Process *process = NODE_UNWRAP(Process, args.Holder());
  assert(process);
  return process->Close() == 0 ? True() : False();
}

Process::Process (Handle<Object> handle)
  : EventEmitter(handle)
{
  ev_init(&stdout_watcher_, Process::OnOutput);
  stdout_watcher_.data = this;

  ev_init(&stderr_watcher_, Process::OnOutput);
  stderr_watcher_.data = this;

  ev_init(&stdin_watcher_, Process::OnWritable);
  stdin_watcher_.data = this;

  ev_init(&child_watcher_, Process::OnCHLD);
  child_watcher_.data = this;

  stdout_pipe_[0] = -1;
  stdout_pipe_[1] = -1;
  stderr_pipe_[0] = -1;
  stderr_pipe_[1] = -1;
  stdin_pipe_[0] = -1;
  stdin_pipe_[1] = -1;

  got_close_ = false;
  got_chld_ = false;
  exit_code_ = 0;

  pid_ = 0; 

  oi_queue_init(&out_stream_);
}

Process::~Process ()
{
  Shutdown();
}

void
Process::Shutdown () 
{
  // Clear the out_stream
  while (!oi_queue_empty(&out_stream_)) {
    oi_queue *q = oi_queue_last(&out_stream_);
    oi_buf *buf = (oi_buf*) oi_queue_data(q, oi_buf, queue);
    oi_queue_remove(q);
    if (buf->release) buf->release(buf);
  }

  if (stdout_pipe_[0] >= 0) close(stdout_pipe_[0]);
  if (stdout_pipe_[1] >= 0) close(stdout_pipe_[1]);

  if (stderr_pipe_[0] >= 0) close(stderr_pipe_[0]);
  if (stderr_pipe_[1] >= 0) close(stderr_pipe_[1]);

  if (stdin_pipe_[0] >= 0) close(stdin_pipe_[0]);
  if (stdin_pipe_[1] >= 0) close(stdin_pipe_[1]);

  stdout_pipe_[0] = -1;
  stdout_pipe_[1] = -1;
  stderr_pipe_[0] = -1;
  stderr_pipe_[1] = -1;
  stdin_pipe_[0] = -1;
  stdin_pipe_[1] = -1;

  ev_io_stop(EV_DEFAULT_UC_ &stdout_watcher_);
  ev_io_stop(EV_DEFAULT_UC_ &stderr_watcher_);
  ev_io_stop(EV_DEFAULT_UC_ &stdin_watcher_);

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
  assert(stdout_pipe_[0] == -1);
  assert(stdout_pipe_[1] == -1);
  assert(stderr_pipe_[0] == -1);
  assert(stderr_pipe_[1] == -1);
  assert(stdin_pipe_[0] == -1);
  assert(stdin_pipe_[1] == -1);

  /* An implementation of popen(), basically */
  if (pipe(stdout_pipe_) < 0) {
    perror("pipe()");
    return -1;
  }

  if (pipe(stderr_pipe_) < 0) {
    perror("pipe()");
    return -2;
  }

  if (pipe(stdin_pipe_) < 0) {
    perror("pipe()");
    return -3;
  }

  switch (pid_ = vfork()) {
    case -1: // Error.
      Shutdown();
      return -4;

    case 0: // Child.
      close(stdout_pipe_[0]); // close read end 
      dup2(stdout_pipe_[1], STDOUT_FILENO);

      close(stderr_pipe_[0]); // close read end 
      dup2(stderr_pipe_[1], STDERR_FILENO);

      close(stdin_pipe_[1]); // close write end 
      dup2(stdin_pipe_[0],  STDIN_FILENO);

      execl("/bin/sh", "sh", "-c", command, (char *)NULL);
      _exit(127);
  }

  // Parent.

  ev_child_set(&child_watcher_, pid_, 0);
  ev_child_start(EV_DEFAULT_UC_ &child_watcher_);

  SetNonBlocking(stdout_pipe_[0]);
  ev_io_set(&stdout_watcher_, stdout_pipe_[0], EV_READ);    
  ev_io_start(EV_DEFAULT_UC_ &stdout_watcher_);
  close(stdout_pipe_[1]); // close write end 
  stdout_pipe_[1] = -1;

  SetNonBlocking(stderr_pipe_[0]);
  ev_io_set(&stderr_watcher_, stderr_pipe_[0], EV_READ);    
  ev_io_start(EV_DEFAULT_UC_ &stderr_watcher_);
  close(stderr_pipe_[1]); // close write end 
  stderr_pipe_[1] = -1;

  SetNonBlocking(stdin_pipe_[1]);
  ev_io_set(&stdin_watcher_, stdin_pipe_[1], EV_WRITE);    
  ev_io_start(EV_DEFAULT_UC_ &stdin_watcher_);
  close(stdin_pipe_[0]);  // close read end 
  stdin_pipe_[0] = -1;

  Attach();

  return 0;
}

void 
Process::OnOutput (EV_P_ ev_io *watcher, int revents)
{
  int r;
  char buf[16*1024];
  size_t buf_size = 16*1024;

  Process *process = static_cast<Process*>(watcher->data);

  bool is_stdout = (&process->stdout_watcher_ == watcher);
  int fd = is_stdout ? process->stdout_pipe_[0] : process->stderr_pipe_[0];

  assert(revents == EV_READ);
  assert(fd >= 0);

  HandleScope scope;
  Handle<Value> argv[1];

  for (;;) {
    r = read(fd, buf, buf_size);

    if (r < 0) {
      if (errno != EAGAIN) {
        perror("IPC pipe read error");
      } else {
        if (process->got_chld_) {
          close(fd);
          if (is_stdout) {
            process->stdout_pipe_[0] = -1;
          } else {
            process->stderr_pipe_[0] = -1;
          }
        }
      }
      break;
    }

    if (r == 0) {
      argv[0] = Null();    
    } else {
      // TODO multiple encodings
      argv[0] = String::New((const char*)buf, r);
    }

    process->Emit(is_stdout ? "Output" : "Error", 1, argv);

    if (r == 0) {
      ev_io_stop(EV_DEFAULT_UC_ watcher);
      close(fd);
      if (is_stdout) {
        process->stdout_pipe_[0] = -1;
      } else {
        process->stderr_pipe_[0] = -1;
      }
      break;
    }
  }
  process->MaybeShutdown();
}

void 
Process::OnWritable (EV_P_ ev_io *watcher, int revents)
{
  Process *process = static_cast<Process*>(watcher->data);
  int sent;

  assert(revents == EV_WRITE);
  assert(process->stdin_pipe_[1] >= 0);

  while (!oi_queue_empty(&process->out_stream_)) {
    oi_queue *q = oi_queue_last(&process->out_stream_);
    oi_buf *to_write = (oi_buf*) oi_queue_data(q, oi_buf, queue);

    sent = write( process->stdin_pipe_[1]
                , to_write->base + to_write->written
                , to_write->len - to_write->written
                );
    if (sent < 0) {
      if (errno == EAGAIN) {
        if (process->got_chld_) {
          close(process->stdin_pipe_[1]);
          process->stdin_pipe_[1] = -1;
        }
        break;
      }
      perror("IPC pipe write error");
      break;
    }

    to_write->written += sent;

    if (to_write->written == to_write->len) {
      oi_queue_remove(q);
      if (to_write->release) to_write->release(to_write);
    }
  }

  if (oi_queue_empty(&process->out_stream_)) {
    ev_io_stop(EV_DEFAULT_UC_ &process->stdin_watcher_);
    if (process->got_close_) {
      close(process->stdin_pipe_[1]);
      process->stdin_pipe_[1] = -1;
    }
  }
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

  if (process->stdout_pipe_[0] >= 0) {
    ev_feed_event(&process->stdout_watcher_, EV_READ);
  }

  if (process->stderr_pipe_[0] >= 0) {
    ev_feed_event(&process->stderr_watcher_, EV_READ);
  }

  if (process->stdin_pipe_[1] >= 0) {
    ev_io_start(EV_DEFAULT_UC_ &process->stdin_watcher_);
    ev_feed_event(&process->stdin_watcher_, EV_WRITE);
  }

  process->MaybeShutdown();
}

int
Process::Write (oi_buf *buf)
{
  if (STDIN_CLOSED || got_close_ || got_chld_) return -1;
  oi_queue_insert_head(&out_stream_, &buf->queue);
  buf->written = 0;
  ev_io_start(EV_DEFAULT_UC_ &stdin_watcher_);
  return 0;
}

int
Process::Close (void)
{
  if (STDIN_CLOSED || got_close_ || got_chld_) return -1;
  got_close_ = true;
  ev_io_start(EV_DEFAULT_UC_ &stdin_watcher_);
  return 0;
}

int
Process::Kill (int sig)
{
  if (got_chld_ || pid_ == 0) return -1;
  return kill(pid_, sig);
}

int
Process::MaybeShutdown (void)
{
  if (STDOUT_CLOSED && STDERR_CLOSED && got_chld_) {
    HandleScope scope;
    Handle<Value> argv[1] = { Integer::New(exit_code_) };
    Emit("Exit", 1, argv);
    Shutdown();
    Detach();
  } 
}
