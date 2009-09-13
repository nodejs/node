// Copyright 2009 Ryan Dahl <ry@tinyclouds.org>
#include <node.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h> /* PATH_MAX */
#include <assert.h>
#include <unistd.h>
#include <errno.h>
#include <dlfcn.h> /* dlopen(), dlsym() */

#include <events.h>
#include <dns.h>
#include <net.h>
#include <file.h>
#include <http.h>
#include <timer.h>
#include <child_process.h>
#include <constants.h>
#include <node_stdio.h>
#include <natives.h>
#include <v8-debug.h>

using namespace v8;

extern char **environ;

namespace node {

static int dash_dash_index = 0;

Local<Value> Encode(const void *buf, size_t len, enum encoding encoding) {
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
    uint16_t * twobytebuf = new uint16_t[len];
    for (size_t i = 0; i < len; i++) {
      // XXX is the following line platform independent?
      twobytebuf[i] = cbuf[i];
    }
    Local<String> chunk = String::New(twobytebuf, len);
    delete twobytebuf;
    return scope.Close(chunk);
  }

  // utf8 or ascii encoding
  Local<String> chunk = String::New((const char*)buf, len);
  return scope.Close(chunk);
}

// Returns -1 if the handle was not valid for decoding
ssize_t DecodeBytes(v8::Handle<v8::Value> val, enum encoding encoding) {
  HandleScope scope;

  if (val->IsArray()) {
    if (encoding != RAW) return -1;
    Handle<Array> array = Handle<Array>::Cast(val);
    return array->Length();
  }

  if (!val->IsString()) return -1;

  Handle<String> str = Handle<String>::Cast(val);

  if (encoding == UTF8) return str->Utf8Length();

  return str->Length();
}

#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

// Returns number of bytes written.
ssize_t DecodeWrite(char *buf, size_t buflen,
                    v8::Handle<v8::Value> val,
                    enum encoding encoding) {
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
  Handle<String> str = Handle<String>::Cast(val);

  if (encoding == UTF8) {
    str->WriteUtf8(buf, buflen);
    return buflen;
  }

  if (encoding == ASCII) {
    str->WriteAscii(buf, 0, buflen);
    return buflen;
  }

  // THIS IS AWFUL!!! FIXME

  uint16_t * twobytebuf = new uint16_t[buflen];

  str->Write(twobytebuf, 0, buflen);

  for (size_t i = 0; i < buflen; i++) {
    unsigned char *b = reinterpret_cast<unsigned char*>(&twobytebuf[i]);
    assert(b[1] == 0);
    buf[i] = b[0];
  }

  delete twobytebuf;

  return buflen;
}

// Extracts a C str from a V8 Utf8Value.
const char* ToCString(const v8::String::Utf8Value& value) {
  return *value ? *value : "<str conversion failed>";
}

static void ReportException(TryCatch *try_catch) {
  Handle<Message> message = try_catch->Message();
  if (message.IsEmpty()) {
    fprintf(stderr, "Error: (no message)\n");
    fflush(stderr);
    return;
  }

  Handle<Value> error = try_catch->Exception();
  Handle<String> stack;

  if (error->IsObject()) {
    Handle<Object> obj = Handle<Object>::Cast(error);
    Handle<Value> raw_stack = obj->Get(String::New("stack"));
    if (raw_stack->IsString()) stack = Handle<String>::Cast(raw_stack);
  }

  // Print (filename):(line number): (message).
  String::Utf8Value filename(message->GetScriptResourceName());
  const char* filename_string = ToCString(filename);
  int linenum = message->GetLineNumber();
  fprintf(stderr, "%s:%i\n", filename_string, linenum);
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

  if (stack.IsEmpty()) {
    message->PrintCurrentStackTrace(stderr);
  } else {
    String::Utf8Value trace(stack);
    fprintf(stderr, "%s\n", *trace);
  }
  fflush(stderr);
}

// Executes a str within the current v8 context.
Handle<Value> ExecuteString(v8::Handle<v8::String> source,
                            v8::Handle<v8::Value> filename) {
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

static Handle<Value> Cwd(const Arguments& args) {
  HandleScope scope;

  char output[PATH_MAX];
  char *r = getcwd(output, PATH_MAX);
  if (r == NULL) {
    return ThrowException(Exception::Error(String::New(strerror(errno))));
  }
  Local<String> cwd = String::New(output);

  return scope.Close(cwd);
}

v8::Handle<v8::Value> Exit(const v8::Arguments& args) {
  int r = 0;
  if (args.Length() > 0)
    r = args[0]->IntegerValue();
  fflush(stderr);
  exit(r);
  return Undefined();
}

typedef void (*extInit)(Handle<Object> exports);

Handle<Value> DLOpen(const v8::Arguments& args) {
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

v8::Handle<v8::Value> Compile(const v8::Arguments& args) {
  HandleScope scope;

  if (args.Length() < 2) {
    return ThrowException(Exception::TypeError(
          String::New("needs two arguments.")));
  }

  Local<String> source = args[0]->ToString();
  Local<String> filename = args[1]->ToString();

  Handle<Script> script = Script::Compile(source, filename);
  if (script.IsEmpty()) return Undefined();

  Handle<Value> result = script->Run();
  if (result.IsEmpty()) return Undefined();

  return scope.Close(result);
}

static void OnFatalError(const char* location, const char* message) {
#define FATAL_ERROR "\033[1;31mV8 FATAL ERROR.\033[m"
  if (location)
    fprintf(stderr, FATAL_ERROR " %s %s\n", location, message);
  else
    fprintf(stderr, FATAL_ERROR " %s\n", message);

  exit(1);
}

void FatalException(TryCatch &try_catch) {
  ReportException(&try_catch);
  exit(1);
}

static ev_async eio_watcher;

static void EIOCallback(EV_P_ ev_async *watcher, int revents) {
  assert(watcher == &eio_watcher);
  assert(revents == EV_ASYNC);
  eio_poll();
}

static void EIOWantPoll(void) {
  ev_async_send(EV_DEFAULT_UC_ &eio_watcher);
}

enum encoding ParseEncoding(Handle<Value> encoding_v, enum encoding _default) {
  HandleScope scope;

  if (!encoding_v->IsString())
    return RAW;

  String::Utf8Value encoding(encoding_v->ToString());

  if (strcasecmp(*encoding, "utf8") == 0) {
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

static void ExecuteNativeJS(const char *filename, const char *data) {
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

static Local<Object> Load(int argc, char *argv[]) {
  HandleScope scope;

  Local<Object> global_obj = Context::GetCurrent()->Global();
  Local<Object> node_obj = Object::New();

  global_obj->Set(String::NewSymbol("node"), node_obj);

  node_obj->Set(String::NewSymbol("version"), String::New(NODE_VERSION));

  int i, j;
  Local<Array> arguments = Array::New(argc - dash_dash_index + 1);
  
  arguments->Set(Integer::New(0), String::New(argv[0]));
  for (j = 1, i = dash_dash_index + 1; i < argc; j++, i++) {
    Local<String> arg = String::New(argv[i]);
    arguments->Set(Integer::New(j), arg);
  }
  global_obj->Set(String::NewSymbol("ARGV"), arguments);

  Local<Object> env = Object::New();
  for (i = 0; environ[i]; i++) {
    for (j = 0; environ[i][j] && environ[i][j] != '='; j++) { ; }
    Local<String> field = String::New(environ[i], j);
    Local<String> value = Local<String>();
    if (environ[i][j] == '=') {
      value = String::New(environ[i]+j+1);
    }
    env->Set(field, value);
  }
  global_obj->Set(String::NewSymbol("ENV"), env);

  NODE_SET_METHOD(node_obj, "compile", Compile);
  NODE_SET_METHOD(node_obj, "reallyExit", Exit);
  NODE_SET_METHOD(node_obj, "cwd", Cwd);
  NODE_SET_METHOD(node_obj, "dlopen", DLOpen);

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

static void CallExitHandler(Handle<Object> node_obj) {
  HandleScope scope;
  Local<Value> exit_v = node_obj->Get(String::New("exit"));
  assert(exit_v->IsFunction());
  Handle<Function> exit_f = Handle<Function>::Cast(exit_v);
  TryCatch try_catch;
  exit_f->Call(Context::GetCurrent()->Global(), 0, NULL);
  if (try_catch.HasCaught())
    node::FatalException(try_catch);
}

static void PrintHelp() {
  printf("Usage: node [options] [--] script.js [arguments] \n"
         "  -v, --version    print node's version\n"
         "  --cflags         print pre-processor and compiler flags\n"
         "  --v8-options     print v8 command line options\n\n"
         "Documentation can be found at http://tinyclouds.org/node/api.html"
         " or with 'man node'\n");
}

static void ParseArgs(int *argc, char **argv) {
  for (int i = 1; i < *argc; i++) {
    const char *arg = argv[i];
    if (strcmp(arg, "--") == 0) {
      dash_dash_index = i;
      break;
    } else if (strcmp(arg, "--version") == 0 || strcmp(arg, "-v") == 0) {
      printf("%s\n", NODE_VERSION);
      exit(0);
    } else if (strcmp(arg, "--cflags") == 0) {
      printf("%s\n", NODE_CFLAGS);
      exit(0);
    } else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
      PrintHelp();
      exit(0);
    } else if (strcmp(arg, "--v8-options") == 0) {
      argv[i] = reinterpret_cast<const char*>("--help");
    }
  }
}

}  // namespace node

int main(int argc, char *argv[]) {
  node::ParseArgs(&argc, argv);
  V8::SetFlagsFromCommandLine(&argc, argv, false);

  evcom_ignore_sigpipe();
  ev_default_loop(EVFLAG_AUTO);  // initialize the default ev loop.

  // start eio thread pool
  ev_async_init(&node::eio_watcher, node::EIOCallback);
  eio_init(node::EIOWantPoll, NULL);
  ev_async_start(EV_DEFAULT_UC_ &node::eio_watcher);
  ev_unref(EV_DEFAULT_UC);

  V8::Initialize();
  V8::SetFatalErrorHandler(node::OnFatalError);

  if (argc < 2)  {
    fprintf(stderr, "No script was specified.\n");
    node::PrintHelp();
    return 1;
  }

  HandleScope handle_scope;

  Local<FunctionTemplate> process_template = FunctionTemplate::New();

  // The global object / "process" is an instance of EventEmitter.  For
  // strange reasons we must initialize EventEmitter now!  it will be assign
  // to it's namespace node.EventEmitter in Load() bellow.
  node::EventEmitter::Initialize(process_template);

  Persistent<Context> context = Context::New(NULL,
      process_template->InstanceTemplate());
  Context::Scope context_scope(context);



  context->Global()->Set(String::NewSymbol("process"), context->Global());

  Local<Object> node_obj = node::Load(argc, argv);

  ev_loop(EV_DEFAULT_UC_ 0);  // main event loop

  node::CallExitHandler(node_obj);

  context.Dispose();
  V8::Dispose();

  return 0;
}

