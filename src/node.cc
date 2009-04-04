#include "node.h"
#define EV_STANDALONE 1
#include <ev.c>

#include "node_tcp.h"
#include "node_http.h"
#include "node_timer.h"

#include <stdio.h>
#include <assert.h>

#include <string>
#include <list>
#include <map>

using namespace v8;
using namespace std;

static int exit_code = 0;

// Reads a file into a v8 string.
static Handle<String>
ReadFile (const string& name) 
{

  FILE* file = fopen(name.c_str(), "rb");
  if (file == NULL) return Handle<String>();
 
  fseek(file, 0, SEEK_END);
  int size = ftell(file);
  rewind(file);

  char chars[size+1];
  chars[size] = '\0';
  for (int i = 0; i < size;) {
    int read = fread(&chars[i], 1, size - i, file);
    if(read <= 0) {
      perror("read()");
    }
    i += read;
  }

  uint16_t expanded_base[size+1];
  expanded_base[size] = '\0';
  for(int i = 0; i < size; i++) 
    expanded_base[i] = chars[i];

  fclose(file);

  HandleScope scope;
  Local<String> result = String::New(expanded_base, size);

  return scope.Close(result);
}

static Handle<Value>
Log (const Arguments& args) 
{
  if (args.Length() < 1) return v8::Undefined();
  HandleScope scope;
  Handle<Value> arg = args[0];
  String::Utf8Value value(arg);

  printf("%s\n", *value);
  fflush(stdout);

  return Undefined();
}


static Handle<Value>
BlockingFileRead (const Arguments& args)
{
  if (args.Length() < 1) return v8::Undefined();
  HandleScope scope;

  String::Utf8Value filename(args[0]);
  Handle<String> output = ReadFile (*filename);
  return scope.Close(output);
}

static void
OnFatalError (const char* location, const char* message)
{
  fprintf(stderr, "Fatal error. %s %s\n", location, message);
  ev_unloop(node_loop(), EVUNLOOP_ALL);
}


// Extracts a C string from a V8 Utf8Value.
const char* ToCString(const v8::String::Utf8Value& value) {
  return *value ? *value : "<string conversion failed>";
}

void ReportException(v8::TryCatch* try_catch) {
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

void
node_fatal_exception (TryCatch &try_catch)
{
  ReportException(&try_catch);
  ev_unloop(node_loop(), EVUNLOOP_ALL);
  exit_code = 1;
}


// Executes a string within the current v8 context.
bool ExecuteString(v8::Handle<v8::String> source,
                   v8::Handle<v8::Value> name,
                   bool print_result,
                   bool report_exceptions) {
  v8::HandleScope handle_scope;
  v8::TryCatch try_catch;
  v8::Handle<v8::Script> script = v8::Script::Compile(source, name);
  if (script.IsEmpty()) {
    // Print errors that happened during compilation.
    if (report_exceptions)
      ReportException(&try_catch);
    return false;
  } else {
    v8::Handle<v8::Value> result = script->Run();
    if (result.IsEmpty()) {
      // Print errors that happened during execution.
      if (report_exceptions)
        ReportException(&try_catch);
      return false;
    } else {
      if (print_result && !result->IsUndefined()) {
        // If all went well and the result wasn't undefined then print
        // the returned value.
        v8::String::Utf8Value str(result);
        const char* cstr = ToCString(str);
        printf("%s\n", cstr);
      }
      return true;
    }
  }
}

// The callback that is invoked by v8 whenever the JavaScript 'load'
// function is called.  Loads, compiles and executes its argument
// JavaScript file.
v8::Handle<v8::Value> Load(const v8::Arguments& args) {
  for (int i = 0; i < args.Length(); i++) {
    v8::HandleScope handle_scope;
    v8::String::Utf8Value file(args[i]);
    if (*file == NULL) {
      return v8::ThrowException(v8::String::New("Error loading file"));
    }
    v8::Handle<v8::String> source = ReadFile(*file);
    if (source.IsEmpty()) {
      return v8::ThrowException(v8::String::New("Error loading file"));
    }
    if (!ExecuteString(source, v8::String::New(*file), false, true)) {
      return v8::ThrowException(v8::String::New("Error executing file"));
    }
  }
  return v8::Undefined();
}

int
main (int argc, char *argv[]) 
{
  V8::SetFlagsFromCommandLine(&argc, argv, true);

  if(argc != 2)  {
    fprintf(stderr, "No script was specified.\n");
    return 1;
  }
  string filename(argv[1]);

  HandleScope handle_scope;

  Persistent<Context> context = Context::New(NULL, ObjectTemplate::New());
  Context::Scope context_scope(context);

  Local<Object> g = Context::GetCurrent()->Global();

  g->Set ( String::New("log")
         , FunctionTemplate::New(Log)->GetFunction()
         );

  g->Set ( String::New("load")
         , FunctionTemplate::New(Load)->GetFunction()
         );

  g->Set ( String::New("blockingFileRead")
         , FunctionTemplate::New(BlockingFileRead)->GetFunction()
         );

  Init_timer(g);
  Init_tcp(g);
  Init_http(g);

  V8::SetFatalErrorHandler(OnFatalError);

  v8::Handle<v8::String> source = ReadFile(filename);
  ExecuteString(source, String::New(filename.c_str()), false, true);

  ev_loop(node_loop(), 0);

  context.Dispose();

  return exit_code;
}

