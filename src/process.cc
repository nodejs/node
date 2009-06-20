#include "node.h"
#include "process.h"

#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>

using namespace v8;
using namespace node;

Persistent<FunctionTemplate> Process::constructor_template;

void
Process::Initialize (Handle<Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(Process::New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);

#if 0
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "start", Timer::Start);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "stop", Timer::Stop);
#endif 

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

Process::Process (Handle<Object> handle)
  : ObjectWrap(handle)
{
  ev_init(&stdout_watcher_, Process::OnOutput);
  stdout_watcher_.data = this;

  ev_init(&stderr_watcher_, Process::OnError);
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

  pid_ = 0; 
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

static int
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

      execl("/bin/sh", "-c", command, (char *)NULL);
      _exit(127);
  }

  // Parent.

  ev_child_set(&child_watcher_, pid_, 0);
  ev_child_start(EV_DEFAULT_UC_ &child_watcher_);

  SetNonBlocking(stdout_pipe_[0]);
  SetNonBlocking(stderr_pipe_[0]);
  SetNonBlocking(stdin_pipe_[1]);

  ev_io_set(&stdout_watcher_, stdout_pipe_[0], EV_READ);    
  ev_io_set(&stderr_watcher_, stderr_pipe_[0], EV_READ);    
  ev_io_set(&stdin_watcher_, stdin_pipe_[1], EV_WRITE);    

  ev_io_start(EV_DEFAULT_UC_ &stdout_watcher_);
  ev_io_start(EV_DEFAULT_UC_ &stderr_watcher_);
  ev_io_start(EV_DEFAULT_UC_ &stdin_watcher_);

  close(stdout_pipe_[1]); // close write end 
  close(stderr_pipe_[1]); // close write end 
  close(stdin_pipe_[0]);  // close read end 

  stdout_pipe_[1] = -1;
  stderr_pipe_[1] = -1;
  stdin_pipe_[0] = -1;

  Attach();

  return 0;
}

void 
Process::OnOutput (EV_P_ ev_io *watcher, int revents)
{
  Process *process = static_cast<Process*>(watcher->data);
  assert(revents == EV_READ);
}

void 
Process::OnError (EV_P_ ev_io *watcher, int revents)
{
  Process *process = static_cast<Process*>(watcher->data);
  assert(revents == EV_READ);
}

void 
Process::OnWritable (EV_P_ ev_io *watcher, int revents)
{
  Process *process = static_cast<Process*>(watcher->data);
  assert(revents == EV_WRITE);
}

void 
Process::OnCHLD (EV_P_ ev_child *watcher, int revents)
{
  ev_child_stop(EV_A_ watcher);
  Process *process = static_cast<Process*>(watcher->data);
  assert(revents == EV_CHILD);
}
