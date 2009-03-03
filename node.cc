#include <oi.h>

#include "node_tcp.h"
#include "node_http.h"
#include "node_timer.h"

#include <stdio.h>
#include <assert.h>
#include <string>
#include <list>
#include <map>

#include <v8.h>

using namespace v8;
using namespace std;

static struct ev_loop *loop;

static Persistent<Context> context;
static Persistent<Function> process_;

// Reads a file into a v8 string.
static Handle<String> ReadFile
  ( const string& name
  ) 
{
  FILE* file = fopen(name.c_str(), "rb");
  if (file == NULL) return Handle<String>();

  fseek(file, 0, SEEK_END);
  int size = ftell(file);
  rewind(file);

  char* chars = new char[size + 1];
  chars[size] = '\0';
  for (int i = 0; i < size;) {
    int read = fread(&chars[i], 1, size - i, file);
    i += read;
  }
  fclose(file);
  Handle<String> result = String::New(chars, size);
  delete[] chars;
  return result;
}

static void ParseOptions
  ( int argc
  , char* argv[]
  , map<string, string>& options
  , string* file
  )
{
  for (int i = 1; i < argc; i++) {
    string arg = argv[i];
    int index = arg.find('=', 0);
    if (index == string::npos) {
      *file = arg;
    } else {
      string key = arg.substr(0, index);
      string value = arg.substr(index+1);
      options[key] = value;
    }
  }
}

static bool compile
  ( Handle<String> script
  ) 
{
  HandleScope handle_scope;

  // We're just about to compile the script; set up an error handler to
  // catch any exceptions the script might throw.
  TryCatch try_catch;

  // Compile the script and check for errors.
  Handle<Script> compiled_script = Script::Compile(script);
  if (compiled_script.IsEmpty()) {

    Handle<Message> message = try_catch.Message();

    String::Utf8Value error(try_catch.Exception());

    printf("error: %s line %d\n", *error, message->GetLineNumber());

    return false;
  }

  // Run the script!
  Handle<Value> result = compiled_script->Run();
  if (result.IsEmpty()) {
    // The TryCatch above is still in effect and will have caught the error.
    String::Utf8Value error(try_catch.Exception());
    printf("error: %s\n", *error);
    // Running the script failed; bail out.
    return false;
  }
  return true;
}

static Handle<Value> LogCallback
  ( const Arguments& args
  ) 
{
  if (args.Length() < 1) return v8::Undefined();
  HandleScope scope;
  Handle<Value> arg = args[0];
  String::Utf8Value value(arg);

  printf("Logged: %s\n", *value);
  fflush(stdout);

  return v8::Undefined();
}

int
main (int argc, char *argv[]) 
{
  loop = ev_default_loop(0);

  map<string, string> options;
  string file;
  ParseOptions(argc, argv, options, &file);
  if (file.empty()) {
    fprintf(stderr, "No script was specified.\n");
    return 1;
  }
  HandleScope scope;
  Handle<String> source = ReadFile(file);
  if (source.IsEmpty()) {
    fprintf(stderr, "Error reading '%s'.\n", file.c_str());
    return 1;
  }

  context = Context::New(NULL, ObjectTemplate::New());
  Context::Scope context_scope(context);

  Local<Object> g = Context::GetCurrent()->Global();
  g->Set( String::New("log"), FunctionTemplate::New(LogCallback)->GetFunction());
  g->Set( String::New("TCP"), node_tcp_initialize(loop));
  g->Set( String::New("HTTP"), node_http_initialize(loop));

  node_timer_initialize(g, loop);


  // Compile and run the script
  if (!compile(source))
    return 1;

  ev_loop(loop, 0);

  context.Dispose();

  return 0;
}
