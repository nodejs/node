#include "node.h"

#include "net.h"
#include "file.h"
#include "http.h"
#include "timer.h"

#include "natives.h" 

#include <stdio.h>
#include <assert.h>

#include <string>
#include <list>
#include <map>

using namespace v8;
using namespace node;
using namespace std;

static int exit_code = 0;

ObjectWrap::~ObjectWrap ( )
{
  if (!handle_.IsEmpty())  {
    handle_->SetInternalField(0, Undefined());
    handle_.Dispose();
    handle_.Clear(); 
  }
}

ObjectWrap::ObjectWrap (Handle<Object> handle)
{
  // TODO throw exception if it's already set
  HandleScope scope;
  handle_ = Persistent<Object>::New(handle);

  Handle<External> external = External::New(this);
  handle_->SetInternalField(0, external);
  handle_.MakeWeak(this, ObjectWrap::MakeWeak);
}

void*
ObjectWrap::Unwrap (v8::Handle<v8::Object> handle)
{
  v8::HandleScope scope;
  v8::Handle<v8::External> field = 
    v8::Handle<v8::External>::Cast(handle->GetInternalField(0));
  return field->Value();
}

void
ObjectWrap::MakeWeak (Persistent<Value> _, void *data)
{
  ObjectWrap *w = static_cast<ObjectWrap*> (data);
  delete w;
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
    ::exit(1);
  }

  Handle<Value> result = script->Run();
  if (result.IsEmpty()) {
    ReportException(&try_catch);
    ::exit(1);
  }

  return scope.Close(result);
}

NODE_METHOD(node_exit)
{
  int r = 0;
  if (args.Length() > 0) 
    r = args[0]->IntegerValue();
  ::exit(r);
  return Undefined(); 
}

NODE_METHOD(compile) 
{
  if (args.Length() < 2) 
    return Undefined();

  HandleScope scope;

  Local<String> source = args[0]->ToString();
  Local<String> filename = args[1]->ToString();

  Handle<Value> result = ExecuteString(source, filename);
  
  return scope.Close(result);
}

NODE_METHOD(debug) 
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

  ::exit(1);
}


void
node::fatal_exception (TryCatch &try_catch)
{
  ReportException(&try_catch);
  ::exit(1);
}

static ev_async thread_pool_watcher;

static void 
thread_pool_cb (EV_P_ ev_async *w, int revents)
{
  int r = eio_poll();
  /* returns 0 if all requests were handled, -1 if not, or the value of EIO_FINISH if != 0 */

  // XXX is this check too heavy? 
  //  it require three locks in eio
  //  what's the better way?
  if (eio_nreqs () == 0 && eio_nready() == 0 && eio_npending() == 0) 
    ev_async_stop(EV_DEFAULT_UC_ w);
}

static void
thread_pool_want_poll (void)
{
  ev_async_send(EV_DEFAULT_UC_ &thread_pool_watcher); 
}

void
node::eio_warmup (void)
{
  ev_async_start(EV_DEFAULT_UC_ &thread_pool_watcher);
}

int
main (int argc, char *argv[]) 
{
  ev_default_loop(EVFLAG_AUTO); // initialize the default ev loop.

  // start eio thread pool
  ev_async_init(&thread_pool_watcher, thread_pool_cb);
  eio_init(thread_pool_want_poll, NULL);

  V8::SetFlagsFromCommandLine(&argc, argv, true);

  if(argc < 2)  {
    fprintf(stderr, "No script was specified.\n");
    return 1;
  }

  string filename(argv[1]);

  HandleScope handle_scope;

  Persistent<Context> context = Context::New(NULL, ObjectTemplate::New());
  Context::Scope context_scope(context);
  V8::SetFatalErrorHandler(OnFatalError);

  Local<Object> g = Context::GetCurrent()->Global();

  Local<Object> node = Object::New();
  g->Set(String::New("node"), node);

  NODE_SET_METHOD(node, "compile", compile);
  NODE_SET_METHOD(node, "debug", debug);
  NODE_SET_METHOD(g, "exit", node_exit);

  Local<Array> arguments = Array::New(argc);
  for (int i = 0; i < argc; i++) {
    Local<String> arg = String::New(argv[i]);
    arguments->Set(Integer::New(i), arg);
  }
  g->Set(String::New("ARGV"), arguments);


  // BUILT-IN MODULES
  Acceptor::Initialize(g);
  Connection::Initialize(g);
  node::Init_timer(g);
  node::Init_file(g);
  node::Init_http(g);

  // NATIVE JAVASCRIPT MODULES
  TryCatch try_catch;

  ExecuteString(String::New(native_file), String::New("file.js"));
  if (try_catch.HasCaught()) goto native_js_error; 

  ExecuteString(String::New(native_main), String::New("main.js"));
  if (try_catch.HasCaught()) goto native_js_error; 

  ev_loop(EV_DEFAULT_UC_ 0);

  context.Dispose();

  return exit_code;

native_js_error:
  ReportException(&try_catch);
  return 1;
}
