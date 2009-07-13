#include "node.h"

#include "events.h"
#include "net.h"
#include "file.h"
#include "http.h"
#include "timer.h"
#include "process.h"
#include "constants.h"

#include "natives.h" 

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <assert.h>

#include <string>
#include <list>
#include <map>

using namespace v8;
using namespace node;
using namespace std;

static void
buf_free (evnet_buf *b)
{
  V8::AdjustAmountOfExternalAllocatedMemory(-b->len);
  free(b);
}

evnet_buf *
node::buf_new (size_t size)
{
  size_t total = sizeof(evnet_buf) + size;
  void *p = malloc(total);
  if (p == NULL) return NULL;

  evnet_buf *b = static_cast<evnet_buf*>(p);
  b->base = static_cast<char*>(p) + sizeof(evnet_buf);

  b->len = size;
  b->release = buf_free;
  V8::AdjustAmountOfExternalAllocatedMemory(total);

  return b;
}

// Extracts a C string from a V8 Utf8Value.
const char*
ToCString(const v8::String::Utf8Value& value)
{
  return *value ? *value : "<string conversion failed>";
}

void
ReportException(v8::TryCatch* try_catch)
{
  v8::HandleScope handle_scope;
  v8::String::Utf8Value exception(try_catch->Exception());
  const char* exception_string = ToCString(exception);
  v8::Handle<v8::Message> message = try_catch->Message();
  if (message.IsEmpty()) {
    // V8 didn't provide any extra information about this error; just
    // print the exception.
    printf("%s\n", exception_string);
  } else {
    message->PrintCurrentStackTrace(stdout);

    // Print (filename):(line number): (message).
    v8::String::Utf8Value filename(message->GetScriptResourceName());
    const char* filename_string = ToCString(filename);
    int linenum = message->GetLineNumber();
    printf("%s:%i: %s\n", filename_string, linenum, exception_string);
    // Print line of source code.
    v8::String::Utf8Value sourceline(message->GetSourceLine());
    const char* sourceline_string = ToCString(sourceline);
    printf("%s\n", sourceline_string);
    // Print wavy underline (GetUnderline is deprecated).
    int start = message->GetStartColumn();
    for (int i = 0; i < start; i++) {
      printf(" ");
    }
    int end = message->GetEndColumn();
    for (int i = start; i < end; i++) {
      printf("^");
    }
    printf("\n");
  }
}

// Executes a string within the current v8 context.
Handle<Value>
ExecuteString(v8::Handle<v8::String> source,
              v8::Handle<v8::Value> filename)
{
  HandleScope scope;
  TryCatch try_catch;

  Handle<Script> script = Script::Compile(source, filename);
  if (script.IsEmpty()) {
    ReportException(&try_catch);
    exit(1);
  }

  Handle<Value> result = script->Run();
  if (result.IsEmpty()) {
    ReportException(&try_catch);
    exit(1);
  }

  return scope.Close(result);
}

v8::Handle<v8::Value>
node_exit (const v8::Arguments& args)
{
  int r = 0;
  if (args.Length() > 0) 
    r = args[0]->IntegerValue();
  exit(r);
  return Undefined(); 
}

v8::Handle<v8::Value>
compile (const v8::Arguments& args)
{
  if (args.Length() < 2) 
    return Undefined();

  HandleScope scope;

  Local<String> source = args[0]->ToString();
  Local<String> filename = args[1]->ToString();

  Handle<Value> result = ExecuteString(source, filename);
  
  return scope.Close(result);
}

v8::Handle<v8::Value>
debug (const v8::Arguments& args)
{
  if (args.Length() < 1) 
    return Undefined();
  HandleScope scope;
  String::Utf8Value msg(args[0]->ToString());
  fprintf(stderr, "DEBUG: %s\n", *msg);
  return Undefined();
}

static void
OnFatalError (const char* location, const char* message)
{

#define FATAL_ERROR "\033[1;31mV8 FATAL ERROR.\033[m"
  if (location)
    fprintf(stderr, FATAL_ERROR " %s %s\n", location, message);
  else 
    fprintf(stderr, FATAL_ERROR " %s\n", message);

  exit(1);
}

void
node::FatalException (TryCatch &try_catch)
{
  ReportException(&try_catch);
  exit(1);
}

static ev_async eio_watcher;

static void 
node_eio_cb (EV_P_ ev_async *watcher, int revents)
{
  assert(watcher == &eio_watcher);
  assert(revents == EV_ASYNC);
  eio_poll();
}

static void
eio_want_poll (void)
{
  ev_async_send(EV_DEFAULT_UC_ &eio_watcher); 
}

enum encoding
node::ParseEncoding (Handle<Value> encoding_v)
{
  HandleScope scope;

  if (!encoding_v->IsString())
    return RAW;

  String::Utf8Value encoding(encoding_v->ToString());

  if(strcasecmp(*encoding, "utf8") == 0) {
    return UTF8;
  } else if (strcasecmp(*encoding, "ascii") == 0) {
    return ASCII;
  } else {
    return RAW;
  }
}

static void
ExecuteNativeJS (const char *filename, const char *data)
{
  HandleScope scope;
  TryCatch try_catch;
  ExecuteString(String::New(data), String::New(filename));
  if (try_catch.HasCaught())  {
    puts("There is an error in Node's built-in javascript");
    puts("This should be reported as a bug!");
    ReportException(&try_catch);
    exit(1);
  }
}

static Local<Object>
Load (int argc, char *argv[])
{
  HandleScope scope;

  Local<Object> global_obj = Context::GetCurrent()->Global();
  Local<Object> node_obj = Object::New();

  global_obj->Set(String::NewSymbol("node"), node_obj);

  Local<Array> arguments = Array::New(argc);
  for (int i = 0; i < argc; i++) {
    Local<String> arg = String::New(argv[i]);
    arguments->Set(Integer::New(i), arg);
  }
  global_obj->Set(String::NewSymbol("ARGV"), arguments);

  NODE_SET_METHOD(node_obj, "compile", compile);
  NODE_SET_METHOD(node_obj, "debug", debug);
  NODE_SET_METHOD(node_obj, "reallyExit", node_exit);

  EventEmitter::Initialize(node_obj);
  Promise::Initialize(node_obj);
  Timer::Initialize(node_obj);
  Process::Initialize(node_obj);

  DefineConstants(node_obj);

  Local<Object> fs = Object::New();
  node_obj->Set(String::NewSymbol("fs"), fs);
  File::Initialize(fs);

  Local<Object> tcp = Object::New();
  node_obj->Set(String::New("tcp"), tcp);
  Server::Initialize(tcp);
  Connection::Initialize(tcp);

  Local<Object> http = Object::New();
  node_obj->Set(String::New("http"), http);
  HTTPServer::Initialize(http);
  HTTPConnection::Initialize(http);

  ExecuteNativeJS("events.js", native_events);
  ExecuteNativeJS("http.js", native_http);
  ExecuteNativeJS("file.js", native_file);
  ExecuteNativeJS("node.js", native_node);

  return scope.Close(node_obj);
}

static void
CallExitHandler (Handle<Object> node_obj)
{
  HandleScope scope;
  Local<Value> exit_v = node_obj->Get(String::New("exit"));
  assert(exit_v->IsFunction());
  Handle<Function> exit_f = Handle<Function>::Cast(exit_v);
  TryCatch try_catch;
  exit_f->Call(Context::GetCurrent()->Global(), 0, NULL);
  if (try_catch.HasCaught())
    node::FatalException(try_catch);
}

int
main (int argc, char *argv[]) 
{
  ev_default_loop(EVFLAG_AUTO); // initialize the default ev loop.

  // start eio thread pool
  ev_async_init(&eio_watcher, node_eio_cb);
  eio_init(eio_want_poll, NULL);
  ev_async_start(EV_DEFAULT_UC_ &eio_watcher);
  ev_unref(EV_DEFAULT_UC);

  V8::SetFlagsFromCommandLine(&argc, argv, true);
  V8::Initialize();
  V8::SetFatalErrorHandler(OnFatalError);

  if(argc < 2)  {
    fprintf(stderr, "No script was specified.\n");
    return 1;
  }

  HandleScope handle_scope;
  Persistent<Context> context = Context::New(NULL, ObjectTemplate::New());
  Context::Scope context_scope(context);

  Local<Object> node_obj = Load(argc, argv);

  ev_loop(EV_DEFAULT_UC_ 0); // main event loop

  CallExitHandler(node_obj);

  context.Dispose();
  V8::Dispose();

  return 0;
}
