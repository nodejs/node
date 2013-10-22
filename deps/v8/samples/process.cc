// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <v8.h>

#include <string>
#include <map>

#ifdef COMPRESS_STARTUP_DATA_BZ2
#error Using compressed startup data is not supported for this sample
#endif

using namespace std;
using namespace v8;

// These interfaces represent an existing request processing interface.
// The idea is to imagine a real application that uses these interfaces
// and then add scripting capabilities that allow you to interact with
// the objects through JavaScript.

/**
 * A simplified http request.
 */
class HttpRequest {
 public:
  virtual ~HttpRequest() { }
  virtual const string& Path() = 0;
  virtual const string& Referrer() = 0;
  virtual const string& Host() = 0;
  virtual const string& UserAgent() = 0;
};


/**
 * The abstract superclass of http request processors.
 */
class HttpRequestProcessor {
 public:
  virtual ~HttpRequestProcessor() { }

  // Initialize this processor.  The map contains options that control
  // how requests should be processed.
  virtual bool Initialize(map<string, string>* options,
                          map<string, string>* output) = 0;

  // Process a single request.
  virtual bool Process(HttpRequest* req) = 0;

  static void Log(const char* event);
};


/**
 * An http request processor that is scriptable using JavaScript.
 */
class JsHttpRequestProcessor : public HttpRequestProcessor {
 public:
  // Creates a new processor that processes requests by invoking the
  // Process function of the JavaScript script given as an argument.
  JsHttpRequestProcessor(Isolate* isolate, Handle<String> script)
      : isolate_(isolate), script_(script) { }
  virtual ~JsHttpRequestProcessor();

  virtual bool Initialize(map<string, string>* opts,
                          map<string, string>* output);
  virtual bool Process(HttpRequest* req);

 private:
  // Execute the script associated with this processor and extract the
  // Process function.  Returns true if this succeeded, otherwise false.
  bool ExecuteScript(Handle<String> script);

  // Wrap the options and output map in a JavaScript objects and
  // install it in the global namespace as 'options' and 'output'.
  bool InstallMaps(map<string, string>* opts, map<string, string>* output);

  // Constructs the template that describes the JavaScript wrapper
  // type for requests.
  static Handle<ObjectTemplate> MakeRequestTemplate(Isolate* isolate);
  static Handle<ObjectTemplate> MakeMapTemplate(Isolate* isolate);

  // Callbacks that access the individual fields of request objects.
  static void GetPath(Local<String> name,
                      const PropertyCallbackInfo<Value>& info);
  static void GetReferrer(Local<String> name,
                          const PropertyCallbackInfo<Value>& info);
  static void GetHost(Local<String> name,
                      const PropertyCallbackInfo<Value>& info);
  static void GetUserAgent(Local<String> name,
                           const PropertyCallbackInfo<Value>& info);

  // Callbacks that access maps
  static void MapGet(Local<String> name,
                     const PropertyCallbackInfo<Value>& info);
  static void MapSet(Local<String> name,
                     Local<Value> value,
                     const PropertyCallbackInfo<Value>& info);

  // Utility methods for wrapping C++ objects as JavaScript objects,
  // and going back again.
  Handle<Object> WrapMap(map<string, string>* obj);
  static map<string, string>* UnwrapMap(Handle<Object> obj);
  Handle<Object> WrapRequest(HttpRequest* obj);
  static HttpRequest* UnwrapRequest(Handle<Object> obj);

  Isolate* GetIsolate() { return isolate_; }

  Isolate* isolate_;
  Handle<String> script_;
  Persistent<Context> context_;
  Persistent<Function> process_;
  static Persistent<ObjectTemplate> request_template_;
  static Persistent<ObjectTemplate> map_template_;
};


// -------------------------
// --- P r o c e s s o r ---
// -------------------------


static void LogCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (args.Length() < 1) return;
  HandleScope scope(args.GetIsolate());
  Handle<Value> arg = args[0];
  String::Utf8Value value(arg);
  HttpRequestProcessor::Log(*value);
}


// Execute the script and fetch the Process method.
bool JsHttpRequestProcessor::Initialize(map<string, string>* opts,
                                        map<string, string>* output) {
  // Create a handle scope to hold the temporary references.
  HandleScope handle_scope(GetIsolate());

  // Create a template for the global object where we set the
  // built-in global functions.
  Handle<ObjectTemplate> global = ObjectTemplate::New();
  global->Set(String::New("log"), FunctionTemplate::New(LogCallback));

  // Each processor gets its own context so different processors don't
  // affect each other. Context::New returns a persistent handle which
  // is what we need for the reference to remain after we return from
  // this method. That persistent handle has to be disposed in the
  // destructor.
  v8::Handle<v8::Context> context = Context::New(GetIsolate(), NULL, global);
  context_.Reset(GetIsolate(), context);

  // Enter the new context so all the following operations take place
  // within it.
  Context::Scope context_scope(context);

  // Make the options mapping available within the context
  if (!InstallMaps(opts, output))
    return false;

  // Compile and run the script
  if (!ExecuteScript(script_))
    return false;

  // The script compiled and ran correctly.  Now we fetch out the
  // Process function from the global object.
  Handle<String> process_name = String::New("Process");
  Handle<Value> process_val = context->Global()->Get(process_name);

  // If there is no Process function, or if it is not a function,
  // bail out
  if (!process_val->IsFunction()) return false;

  // It is a function; cast it to a Function
  Handle<Function> process_fun = Handle<Function>::Cast(process_val);

  // Store the function in a Persistent handle, since we also want
  // that to remain after this call returns
  process_.Reset(GetIsolate(), process_fun);

  // All done; all went well
  return true;
}


bool JsHttpRequestProcessor::ExecuteScript(Handle<String> script) {
  HandleScope handle_scope(GetIsolate());

  // We're just about to compile the script; set up an error handler to
  // catch any exceptions the script might throw.
  TryCatch try_catch;

  // Compile the script and check for errors.
  Handle<Script> compiled_script = Script::Compile(script);
  if (compiled_script.IsEmpty()) {
    String::Utf8Value error(try_catch.Exception());
    Log(*error);
    // The script failed to compile; bail out.
    return false;
  }

  // Run the script!
  Handle<Value> result = compiled_script->Run();
  if (result.IsEmpty()) {
    // The TryCatch above is still in effect and will have caught the error.
    String::Utf8Value error(try_catch.Exception());
    Log(*error);
    // Running the script failed; bail out.
    return false;
  }
  return true;
}


bool JsHttpRequestProcessor::InstallMaps(map<string, string>* opts,
                                         map<string, string>* output) {
  HandleScope handle_scope(GetIsolate());

  // Wrap the map object in a JavaScript wrapper
  Handle<Object> opts_obj = WrapMap(opts);

  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(GetIsolate(), context_);

  // Set the options object as a property on the global object.
  context->Global()->Set(String::New("options"), opts_obj);

  Handle<Object> output_obj = WrapMap(output);
  context->Global()->Set(String::New("output"), output_obj);

  return true;
}


bool JsHttpRequestProcessor::Process(HttpRequest* request) {
  // Create a handle scope to keep the temporary object references.
  HandleScope handle_scope(GetIsolate());

  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(GetIsolate(), context_);

  // Enter this processor's context so all the remaining operations
  // take place there
  Context::Scope context_scope(context);

  // Wrap the C++ request object in a JavaScript wrapper
  Handle<Object> request_obj = WrapRequest(request);

  // Set up an exception handler before calling the Process function
  TryCatch try_catch;

  // Invoke the process function, giving the global object as 'this'
  // and one argument, the request.
  const int argc = 1;
  Handle<Value> argv[argc] = { request_obj };
  v8::Local<v8::Function> process =
      v8::Local<v8::Function>::New(GetIsolate(), process_);
  Handle<Value> result = process->Call(context->Global(), argc, argv);
  if (result.IsEmpty()) {
    String::Utf8Value error(try_catch.Exception());
    Log(*error);
    return false;
  } else {
    return true;
  }
}


JsHttpRequestProcessor::~JsHttpRequestProcessor() {
  // Dispose the persistent handles.  When noone else has any
  // references to the objects stored in the handles they will be
  // automatically reclaimed.
  context_.Dispose();
  process_.Dispose();
}


Persistent<ObjectTemplate> JsHttpRequestProcessor::request_template_;
Persistent<ObjectTemplate> JsHttpRequestProcessor::map_template_;


// -----------------------------------
// --- A c c e s s i n g   M a p s ---
// -----------------------------------

// Utility function that wraps a C++ http request object in a
// JavaScript object.
Handle<Object> JsHttpRequestProcessor::WrapMap(map<string, string>* obj) {
  // Handle scope for temporary handles.
  HandleScope handle_scope(GetIsolate());

  // Fetch the template for creating JavaScript map wrappers.
  // It only has to be created once, which we do on demand.
  if (map_template_.IsEmpty()) {
    Handle<ObjectTemplate> raw_template = MakeMapTemplate(GetIsolate());
    map_template_.Reset(GetIsolate(), raw_template);
  }
  Handle<ObjectTemplate> templ =
      Local<ObjectTemplate>::New(GetIsolate(), map_template_);

  // Create an empty map wrapper.
  Handle<Object> result = templ->NewInstance();

  // Wrap the raw C++ pointer in an External so it can be referenced
  // from within JavaScript.
  Handle<External> map_ptr = External::New(obj);

  // Store the map pointer in the JavaScript wrapper.
  result->SetInternalField(0, map_ptr);

  // Return the result through the current handle scope.  Since each
  // of these handles will go away when the handle scope is deleted
  // we need to call Close to let one, the result, escape into the
  // outer handle scope.
  return handle_scope.Close(result);
}


// Utility function that extracts the C++ map pointer from a wrapper
// object.
map<string, string>* JsHttpRequestProcessor::UnwrapMap(Handle<Object> obj) {
  Handle<External> field = Handle<External>::Cast(obj->GetInternalField(0));
  void* ptr = field->Value();
  return static_cast<map<string, string>*>(ptr);
}


// Convert a JavaScript string to a std::string.  To not bother too
// much with string encodings we just use ascii.
string ObjectToString(Local<Value> value) {
  String::Utf8Value utf8_value(value);
  return string(*utf8_value);
}


void JsHttpRequestProcessor::MapGet(Local<String> name,
                                    const PropertyCallbackInfo<Value>& info) {
  // Fetch the map wrapped by this object.
  map<string, string>* obj = UnwrapMap(info.Holder());

  // Convert the JavaScript string to a std::string.
  string key = ObjectToString(name);

  // Look up the value if it exists using the standard STL ideom.
  map<string, string>::iterator iter = obj->find(key);

  // If the key is not present return an empty handle as signal
  if (iter == obj->end()) return;

  // Otherwise fetch the value and wrap it in a JavaScript string
  const string& value = (*iter).second;
  info.GetReturnValue().Set(
      String::New(value.c_str(), static_cast<int>(value.length())));
}


void JsHttpRequestProcessor::MapSet(Local<String> name,
                                    Local<Value> value_obj,
                                    const PropertyCallbackInfo<Value>& info) {
  // Fetch the map wrapped by this object.
  map<string, string>* obj = UnwrapMap(info.Holder());

  // Convert the key and value to std::strings.
  string key = ObjectToString(name);
  string value = ObjectToString(value_obj);

  // Update the map.
  (*obj)[key] = value;

  // Return the value; any non-empty handle will work.
  info.GetReturnValue().Set(value_obj);
}


Handle<ObjectTemplate> JsHttpRequestProcessor::MakeMapTemplate(
    Isolate* isolate) {
  HandleScope handle_scope(isolate);

  Handle<ObjectTemplate> result = ObjectTemplate::New();
  result->SetInternalFieldCount(1);
  result->SetNamedPropertyHandler(MapGet, MapSet);

  // Again, return the result through the current handle scope.
  return handle_scope.Close(result);
}


// -------------------------------------------
// --- A c c e s s i n g   R e q u e s t s ---
// -------------------------------------------

/**
 * Utility function that wraps a C++ http request object in a
 * JavaScript object.
 */
Handle<Object> JsHttpRequestProcessor::WrapRequest(HttpRequest* request) {
  // Handle scope for temporary handles.
  HandleScope handle_scope(GetIsolate());

  // Fetch the template for creating JavaScript http request wrappers.
  // It only has to be created once, which we do on demand.
  if (request_template_.IsEmpty()) {
    Handle<ObjectTemplate> raw_template = MakeRequestTemplate(GetIsolate());
    request_template_.Reset(GetIsolate(), raw_template);
  }
  Handle<ObjectTemplate> templ =
      Local<ObjectTemplate>::New(GetIsolate(), request_template_);

  // Create an empty http request wrapper.
  Handle<Object> result = templ->NewInstance();

  // Wrap the raw C++ pointer in an External so it can be referenced
  // from within JavaScript.
  Handle<External> request_ptr = External::New(request);

  // Store the request pointer in the JavaScript wrapper.
  result->SetInternalField(0, request_ptr);

  // Return the result through the current handle scope.  Since each
  // of these handles will go away when the handle scope is deleted
  // we need to call Close to let one, the result, escape into the
  // outer handle scope.
  return handle_scope.Close(result);
}


/**
 * Utility function that extracts the C++ http request object from a
 * wrapper object.
 */
HttpRequest* JsHttpRequestProcessor::UnwrapRequest(Handle<Object> obj) {
  Handle<External> field = Handle<External>::Cast(obj->GetInternalField(0));
  void* ptr = field->Value();
  return static_cast<HttpRequest*>(ptr);
}


void JsHttpRequestProcessor::GetPath(Local<String> name,
                                     const PropertyCallbackInfo<Value>& info) {
  // Extract the C++ request object from the JavaScript wrapper.
  HttpRequest* request = UnwrapRequest(info.Holder());

  // Fetch the path.
  const string& path = request->Path();

  // Wrap the result in a JavaScript string and return it.
  info.GetReturnValue().Set(
      String::New(path.c_str(), static_cast<int>(path.length())));
}


void JsHttpRequestProcessor::GetReferrer(
    Local<String> name,
    const PropertyCallbackInfo<Value>& info) {
  HttpRequest* request = UnwrapRequest(info.Holder());
  const string& path = request->Referrer();
  info.GetReturnValue().Set(
      String::New(path.c_str(), static_cast<int>(path.length())));
}


void JsHttpRequestProcessor::GetHost(Local<String> name,
                                     const PropertyCallbackInfo<Value>& info) {
  HttpRequest* request = UnwrapRequest(info.Holder());
  const string& path = request->Host();
  info.GetReturnValue().Set(
      String::New(path.c_str(), static_cast<int>(path.length())));
}


void JsHttpRequestProcessor::GetUserAgent(
    Local<String> name,
    const PropertyCallbackInfo<Value>& info) {
  HttpRequest* request = UnwrapRequest(info.Holder());
  const string& path = request->UserAgent();
  info.GetReturnValue().Set(
      String::New(path.c_str(), static_cast<int>(path.length())));
}


Handle<ObjectTemplate> JsHttpRequestProcessor::MakeRequestTemplate(
    Isolate* isolate) {
  HandleScope handle_scope(isolate);

  Handle<ObjectTemplate> result = ObjectTemplate::New();
  result->SetInternalFieldCount(1);

  // Add accessors for each of the fields of the request.
  result->SetAccessor(String::NewSymbol("path"), GetPath);
  result->SetAccessor(String::NewSymbol("referrer"), GetReferrer);
  result->SetAccessor(String::NewSymbol("host"), GetHost);
  result->SetAccessor(String::NewSymbol("userAgent"), GetUserAgent);

  // Again, return the result through the current handle scope.
  return handle_scope.Close(result);
}


// --- Test ---


void HttpRequestProcessor::Log(const char* event) {
  printf("Logged: %s\n", event);
}


/**
 * A simplified http request.
 */
class StringHttpRequest : public HttpRequest {
 public:
  StringHttpRequest(const string& path,
                    const string& referrer,
                    const string& host,
                    const string& user_agent);
  virtual const string& Path() { return path_; }
  virtual const string& Referrer() { return referrer_; }
  virtual const string& Host() { return host_; }
  virtual const string& UserAgent() { return user_agent_; }
 private:
  string path_;
  string referrer_;
  string host_;
  string user_agent_;
};


StringHttpRequest::StringHttpRequest(const string& path,
                                     const string& referrer,
                                     const string& host,
                                     const string& user_agent)
    : path_(path),
      referrer_(referrer),
      host_(host),
      user_agent_(user_agent) { }


void ParseOptions(int argc,
                  char* argv[],
                  map<string, string>& options,
                  string* file) {
  for (int i = 1; i < argc; i++) {
    string arg = argv[i];
    size_t index = arg.find('=', 0);
    if (index == string::npos) {
      *file = arg;
    } else {
      string key = arg.substr(0, index);
      string value = arg.substr(index+1);
      options[key] = value;
    }
  }
}


// Reads a file into a v8 string.
Handle<String> ReadFile(const string& name) {
  FILE* file = fopen(name.c_str(), "rb");
  if (file == NULL) return Handle<String>();

  fseek(file, 0, SEEK_END);
  int size = ftell(file);
  rewind(file);

  char* chars = new char[size + 1];
  chars[size] = '\0';
  for (int i = 0; i < size;) {
    int read = static_cast<int>(fread(&chars[i], 1, size - i, file));
    i += read;
  }
  fclose(file);
  Handle<String> result = String::New(chars, size);
  delete[] chars;
  return result;
}


const int kSampleSize = 6;
StringHttpRequest kSampleRequests[kSampleSize] = {
  StringHttpRequest("/process.cc", "localhost", "google.com", "firefox"),
  StringHttpRequest("/", "localhost", "google.net", "firefox"),
  StringHttpRequest("/", "localhost", "google.org", "safari"),
  StringHttpRequest("/", "localhost", "yahoo.com", "ie"),
  StringHttpRequest("/", "localhost", "yahoo.com", "safari"),
  StringHttpRequest("/", "localhost", "yahoo.com", "firefox")
};


bool ProcessEntries(HttpRequestProcessor* processor, int count,
                    StringHttpRequest* reqs) {
  for (int i = 0; i < count; i++) {
    if (!processor->Process(&reqs[i]))
      return false;
  }
  return true;
}


void PrintMap(map<string, string>* m) {
  for (map<string, string>::iterator i = m->begin(); i != m->end(); i++) {
    pair<string, string> entry = *i;
    printf("%s: %s\n", entry.first.c_str(), entry.second.c_str());
  }
}


int main(int argc, char* argv[]) {
  v8::V8::InitializeICU();
  map<string, string> options;
  string file;
  ParseOptions(argc, argv, options, &file);
  if (file.empty()) {
    fprintf(stderr, "No script was specified.\n");
    return 1;
  }
  Isolate* isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);
  Handle<String> source = ReadFile(file);
  if (source.IsEmpty()) {
    fprintf(stderr, "Error reading '%s'.\n", file.c_str());
    return 1;
  }
  JsHttpRequestProcessor processor(isolate, source);
  map<string, string> output;
  if (!processor.Initialize(&options, &output)) {
    fprintf(stderr, "Error initializing processor.\n");
    return 1;
  }
  if (!ProcessEntries(&processor, kSampleSize, kSampleRequests))
    return 1;
  PrintMap(&output);
}
