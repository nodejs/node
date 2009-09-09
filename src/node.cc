#include "node.h"

#include "events.h"
#include "dns.h"
#include "net.h"
#include "file.h"
#include "http.h"
#include "timer.h"
#include "child_process.h"
#include "constants.h"
#include "node_stdio.h"

#include "natives.h"

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <limits.h> /* PATH_MAX */
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <dlfcn.h> /* dlopen(), dlsym() */

#include <string>
#include <list>
#include <map>

#include <v8-debug.h>

using namespace v8;
using namespace node;

Local<Value>
node::Encode (const void *buf, size_t len, enum encoding encoding)
{
  HandleScope scope;

  if (!len) return scope.Close(Null());

  if (encoding == RAW) {
    // raw encoding
    Local<Array> array = Array::New(len);
    for (size_t i = 0; i < len; i++) {
      unsigned char val = static_cast<const unsigned char*>(buf)[i];
      array->Set(Integer::New(i), Integer::New(val));
    }
    return scope.Close(array);
  } 

  if (encoding == RAWS) {
    const unsigned char *cbuf = static_cast<const unsigned char*>(buf);
    uint16_t twobytebuf[len];
    for (size_t i = 0; i < len; i++) {
      // XXX is the following line platform independent? 
      twobytebuf[i] = cbuf[i];
    }
    Local<String> chunk = String::New(twobytebuf, len);
    return scope.Close(chunk);
  }

  // utf8 or ascii encoding
  Local<String> chunk = String::New((const char*)buf, len);
  return scope.Close(chunk);
}

// Returns -1 if the handle was not valid for decoding 
ssize_t
node::DecodeBytes (v8::Handle<v8::Value> val, enum encoding encoding)
{
  HandleScope scope;

  if (val->IsArray()) {
    if (encoding != RAW) return -1;
    Handle<Array> array = Handle<Array>::Cast(val);
    return array->Length();
  }

  if (!val->IsString()) return -1;

  Handle<String> string = Handle<String>::Cast(val);

  if (encoding == UTF8) return string->Utf8Length();

  return string->Length();
}

#ifndef MIN
# define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

// Returns number of bytes written.
ssize_t
node::DecodeWrite (char *buf, size_t buflen, v8::Handle<v8::Value> val, enum encoding encoding)
{
  size_t i;
  HandleScope scope;

  // XXX
  // A lot of improvement can be made here. See:
  // http://code.google.com/p/v8/issues/detail?id=270
  // http://groups.google.com/group/v8-dev/browse_thread/thread/dba28a81d9215291/ece2b50a3b4022c
  // http://groups.google.com/group/v8-users/browse_thread/thread/1f83b0ba1f0a611

  if (val->IsArray()) {
    if (encoding != RAW) return -1;
    Handle<Array> array = Handle<Array>::Cast(val);
    size_t array_len = array->Length();
    for (i = 0; i < MIN(buflen, array_len); i++) {
      Local<Value> int_value = array->Get(Integer::New(i));
      buf[i] = int_value->IntegerValue();
    }
    return i;
  }

  assert(val->IsString());
  Handle<String> string = Handle<String>::Cast(val);

  if (encoding == UTF8) {
    string->WriteUtf8(buf, buflen);
    return buflen;
  }

  if (encoding == ASCII) {
    string->WriteAscii(buf, 0, buflen);
    return buflen;
  }

  // THIS IS AWFUL!!! FIXME

  uint16_t twobytebuf[buflen];

  string->Write(twobytebuf, 0, buflen);

  for (size_t i = 0; i < buflen; i++) {
    unsigned char *b = reinterpret_cast<unsigned char*>(&twobytebuf[i]);
    assert(b[1] == 0);
    buf[i] = b[0];
  }

  return buflen;
}

// Extracts a C string from a V8 Utf8Value.
const char*
ToCString(const v8::String::Utf8Value& value)
{
  return *value ? *value : "<string conversion failed>";
}

static void
ReportException (TryCatch &try_catch)
{
  Handle<Message> message = try_catch.Message();
  if (message.IsEmpty()) {
    fprintf(stderr, "Error: (no message)\n");
    return;
  }
  Handle<Value> error = try_catch.Exception();
  Handle<String> stack;
  if (error->IsObject()) {
    Handle<Object> obj = Handle<Object>::Cast(error);
    Handle<Value> raw_stack = obj->Get(String::New("stack"));
    if (raw_stack->IsString()) stack = Handle<String>::Cast(raw_stack);
  }
  if (stack.IsEmpty()) {
    String::Utf8Value exception(error);

    // Print (filename):(line number): (message).
    String::Utf8Value filename(message->GetScriptResourceName());
    const char* filename_string = ToCString(filename);
    int linenum = message->GetLineNumber();
    fprintf(stderr, "%s:%i: %s\n", filename_string, linenum, *exception);
    // Print line of source code.
    String::Utf8Value sourceline(message->GetSourceLine());
    const char* sourceline_string = ToCString(sourceline);
    fprintf(stderr, "%s\n", sourceline_string);
    // Print wavy underline (GetUnderline is deprecated).
    int start = message->GetStartColumn();
    for (int i = 0; i < start; i++) {
      fprintf(stderr, " ");
    }
    int end = message->GetEndColumn();
    for (int i = start; i < end; i++) {
      fprintf(stderr, "^");
    }
    fprintf(stderr, "\n");

    message->PrintCurrentStackTrace(stderr);


  } else {
    String::Utf8Value trace(stack);
    fprintf(stderr, "%s\n", *trace);
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
    ReportException(try_catch);
    exit(1);
  }

  Handle<Value> result = script->Run();
  if (result.IsEmpty()) {
    ReportException(try_catch);
    exit(1);
  }

  return scope.Close(result);
}

static Handle<Value>
Cwd (const Arguments& args)
{
  HandleScope scope;

  char output[PATH_MAX];
  char *r = getcwd(output, PATH_MAX);
  if (r == NULL) {
    return ThrowException(Exception::Error(String::New(strerror(errno))));
  }
  Local<String> cwd = String::New(output);

  return scope.Close(cwd);
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

typedef void (*extInit)(Handle<Object> exports);

Handle<Value>
node_dlopen (const v8::Arguments& args)
{
  HandleScope scope;

  if (args.Length() < 2) return Undefined();

  String::Utf8Value filename(args[0]->ToString());
  Local<Object> target = args[1]->ToObject();

  void *handle = dlopen(*filename, RTLD_LAZY);

  if (handle == NULL) {
    Local<Value> exception = Exception::Error(String::New(dlerror()));
    return ThrowException(exception);
  }

  void *init_handle = dlsym(handle, "init");
  if (init_handle == NULL) {
    Local<Value> exception =
      Exception::Error(String::New("No 'init' symbol found in module."));
    return ThrowException(exception);
  }
  extInit init = reinterpret_cast<extInit>(init_handle);

  init(target);

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
  ReportException(try_catch);
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
node::ParseEncoding (Handle<Value> encoding_v, enum encoding _default)
{
  HandleScope scope;

  if (!encoding_v->IsString())
    return RAW;

  String::Utf8Value encoding(encoding_v->ToString());

  if(strcasecmp(*encoding, "utf8") == 0) {
    return UTF8;
  } else if (strcasecmp(*encoding, "ascii") == 0) {
    return ASCII;
  } else if (strcasecmp(*encoding, "raw") == 0) {
    return RAW;
  } else if (strcasecmp(*encoding, "raws") == 0) {
    return RAWS;
  } else {
    return _default;
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
    ReportException(try_catch);
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

  node_obj->Set(String::NewSymbol("version"), String::New(NODE_VERSION));

  Local<Array> arguments = Array::New(argc);
  for (int i = 0; i < argc; i++) {
    Local<String> arg = String::New(argv[i]);
    arguments->Set(Integer::New(i), arg);
  }
  global_obj->Set(String::NewSymbol("ARGV"), arguments);

  NODE_SET_METHOD(node_obj, "compile", compile);
  NODE_SET_METHOD(node_obj, "reallyExit", node_exit);
  NODE_SET_METHOD(node_obj, "cwd", Cwd);
  NODE_SET_METHOD(node_obj, "dlopen", node_dlopen);

  node_obj->Set(String::NewSymbol("EventEmitter"),
              EventEmitter::constructor_template->GetFunction());
  Promise::Initialize(node_obj);

  Stdio::Initialize(node_obj);
  Timer::Initialize(node_obj);
  ChildProcess::Initialize(node_obj);

  DefineConstants(node_obj);

  Local<Object> dns = Object::New();
  node_obj->Set(String::NewSymbol("dns"), dns);
  DNS::Initialize(dns);

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

  ExecuteNativeJS("util.js", native_util);
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

static void
PrintHelp ( )
{
  printf("Usage: node [switches] script.js [arguments] \n"
         "  -v, --version    print node's version\n"
         "  --cflags         print pre-processor and compiler flags\n"
         "  --v8-options     print v8 command line options\n");
}

static void
ParseArgs (int *argc, char **argv)
{
  for (int i = 1; i < *argc; i++) {
    const char *arg = argv[i];
    if (strcmp(arg, "--version") == 0 || strcmp(arg, "-v") == 0) {
      printf("%s\n", NODE_VERSION);
      exit(0);
    } else if (strcmp(arg, "--cflags") == 0) {
      printf("%s\n", NODE_CFLAGS);
      exit(0);
    } else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
      PrintHelp();
      exit(0);
    } else if (strcmp(arg, "--v8-options") == 0) {
      argv[i] = (char*)"--help";
    }
  }
}

int
main(int argc, char *argv[])
{
  evcom_ignore_sigpipe();
  ev_default_loop(EVFLAG_AUTO); // initialize the default ev loop.

  // start eio thread pool
  ev_async_init(&eio_watcher, node_eio_cb);
  eio_init(eio_want_poll, NULL);
  ev_async_start(EV_DEFAULT_UC_ &eio_watcher);
  ev_unref(EV_DEFAULT_UC);

  ParseArgs(&argc, argv);
  V8::SetFlagsFromCommandLine(&argc, argv, true);
  V8::Initialize();
  V8::SetFatalErrorHandler(OnFatalError);

  if(argc < 2)  {
    fprintf(stderr, "No script was specified.\n");
    PrintHelp();
    return 1;
  }

  HandleScope handle_scope;

  Local<FunctionTemplate> process_template = FunctionTemplate::New();

  // The global object / "process" is an instance of EventEmitter.  For
  // strange reasons we must initialize EventEmitter now!  it will be assign
  // to it's namespace node.EventEmitter in Load() bellow.
  EventEmitter::Initialize(process_template);

  Debug::EnableAgent("node", 2222);

  Persistent<Context> context = Context::New(NULL,
      process_template->InstanceTemplate());
  Context::Scope context_scope(context);



  context->Global()->Set(String::NewSymbol("process"), context->Global());

  Local<Object> node_obj = Load(argc, argv);

  ev_loop(EV_DEFAULT_UC_ 0); // main event loop

  CallExitHandler(node_obj);

  context.Dispose();
  V8::Dispose();

  return 0;
}
