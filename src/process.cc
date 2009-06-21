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

Persistent<FunctionTemplate> Process::constructor_template;

void
Process::Initialize (Handle<Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(Process::New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "write", Process::Write);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "close", Process::Close);

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

static void
free_buf (oi_buf *b)
{
  V8::AdjustAmountOfExternalAllocatedMemory(-b->len);
  free(b);
}

static oi_buf *
new_buf (size_t size)
{
  size_t total = sizeof(oi_buf) + size;
  void *p = malloc(total);
  if (p == NULL) return NULL;

  oi_buf *b = static_cast<oi_buf*>(p);
  b->base = static_cast<char*>(p) + sizeof(oi_buf);

  b->len = size;
  b->release = free_buf;
  V8::AdjustAmountOfExternalAllocatedMemory(total);

  return b;
}

Handle<Value>
Process::Write (const Arguments& args)
{
  HandleScope scope;
  Process *process = NODE_UNWRAP(Process, args.Holder());
  assert(process);

#if 0
  if ( connection->ReadyState() != OPEN 
    && connection->ReadyState() != WRITE_ONLY
     ) 
    return ThrowException(String::New("Socket is not open for writing"));
#endif

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
    buf = new_buf(len);
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
    buf = new_buf(len);
    for (size_t i = 0; i < len; i++) {
      Local<Value> int_value = array->Get(Integer::New(i));
      buf->base[i] = int_value->IntegerValue();
    }

  } else return ThrowException(String::New("Bad argument"));

  if (process->Write(buf) != 0) {
    return ThrowException(String::New("Pipe already closed"));
  }

  return Undefined();  
}

Handle<Value>
Process::Close (const Arguments& args)
{
  HandleScope scope;
  Process *process = NODE_UNWRAP(Process, args.Holder());
  assert(process);

  if (process->Close() != 0) {
    return ThrowException(String::New("Pipe already closed."));
  }

  return Undefined();  
}

Process::Process (Handle<Object> handle)
  : ObjectWrap(handle)
{
  ev_init(&stdout_watcher_, Process::OnOutput);
  stdout_watcher_.data = this;

  ev_init(&stderr_watcher_, Process::OnOutput);
  stderr_watcher_.data = this;

  ev_init(&stdin_watcher_, Process::OnWritable);
  stdin_watcher_.data = this;

  ev_init(&child_watcher_, Process::OnExit);
  child_watcher_.data = this;

  stdout_pipe_[0] = -1;
  stdout_pipe_[1] = -1;
  stderr_pipe_[0] = -1;
  stderr_pipe_[1] = -1;
  stdin_pipe_[0] = -1;
  stdin_pipe_[1] = -1;

  got_close_ = false;

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

  Detach();
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
      //printf("child process!\n");

      close(stdout_pipe_[0]); // close read end 
      dup2(stdout_pipe_[1], STDOUT_FILENO);

      close(stderr_pipe_[0]); // close read end 
      dup2(stderr_pipe_[1], STDERR_FILENO);

      close(stdin_pipe_[1]); // close write end 
      dup2(stdin_pipe_[0],  STDIN_FILENO);

      //printf("child process!\n");

      execl("/bin/sh", "sh", "-c", command, (char *)NULL);
      //execl(_PATH_BSHELL, "sh", "-c", program, (char *)NULL);
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
  Handle<Value> callback_v =
    process->handle_->Get(is_stdout ? ON_OUTPUT_SYMBOL : ON_ERROR_SYMBOL);
  Handle<Function> callback;
  if (callback_v->IsFunction()) {
    callback = Handle<Function>::Cast(callback_v);
  }
  Handle<Value> argv[1];

  for (;;) {
    r = read(fd, buf, buf_size);

    if (r < 0) {
      if (errno != EAGAIN) perror("IPC pipe read error");
      break;
    }

    if (!callback.IsEmpty()) {
      if (r == 0) {
        argv[0] = Null();    
      } else {
        // TODO multiple encodings
        argv[0] = String::New((const char*)buf, r);
      }

      TryCatch try_catch;
      callback->Call(process->handle_, 1, argv);
      if (try_catch.HasCaught()) {
        FatalException(try_catch);
        return;
      }
    }

    if (r == 0) {
      ev_io_stop(EV_DEFAULT_UC_ watcher);
      break;
    }
  }
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
      if (errno == EAGAIN) break;
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
Process::OnExit (EV_P_ ev_child *watcher, int revents)
{
  ev_child_stop(EV_A_ watcher);
  Process *process = static_cast<Process*>(watcher->data);

  assert(revents == EV_CHILD);
  assert(process->pid_ == watcher->rpid);
  assert(&process->child_watcher_ == watcher);

  // Call onExit ( watcher->rstatus )
  HandleScope scope;
  Handle<Value> callback_v = process->handle_->Get(ON_EXIT_SYMBOL);

  if (callback_v->IsFunction()) {
    Handle<Function> callback = Handle<Function>::Cast(callback_v);
    TryCatch try_catch;
    Handle<Value> argv[1] = { Integer::New(watcher->rstatus) };
    callback->Call(process->handle_, 1, argv);
    if (try_catch.HasCaught()) FatalException(try_catch);
  }
  process->Shutdown();
  process->Detach();
}

int
Process::Write (oi_buf *buf)
{
  if (stdin_pipe_[1] < 0 || got_close_)
    return -1;
  oi_queue_insert_head(&out_stream_, &buf->queue);
  buf->written = 0;
  ev_io_start(EV_DEFAULT_UC_ &stdin_watcher_);
  return 0;
}

int
Process::Close ()
{
  if (stdin_pipe_[1] < 0 || got_close_)
    return -1;
  got_close_ = true;
  ev_io_start(EV_DEFAULT_UC_ &stdin_watcher_);
  return 0;
}

