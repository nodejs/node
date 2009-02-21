// Copyright 2008 the V8 project authors. All rights reserved.
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

// To avoid warnings from <map> on windows we disable exceptions.
#define _HAS_EXCEPTIONS 0
#include <string>
#include <map>

#include "js_http_request_processor.h"

using namespace std;
using namespace v8;

static Handle<Value> LogCallback
  ( const Arguments& args
  ) 
{
  if (args.Length() < 1) return v8::Undefined();
  HandleScope scope;
  Handle<Value> arg = args[0];
  String::Utf8Value value(arg);
  HttpRequestProcessor::Log(*value);
  return v8::Undefined();
}


// Execute the script and fetch the Process method.
bool JsHttpRequestProcessor::Initialize
  ( map<string, string>* opts
  , map<string, string>* output
  ) 
{
  // Create a handle scope to hold the temporary references.
  HandleScope handle_scope;

  // Create a template for the global object where we set the
  // built-in global functions.
  Handle<ObjectTemplate> global = ObjectTemplate::New();
  global->Set(String::New("log"), FunctionTemplate::New(LogCallback));

  // Each processor gets its own context so different processors
  // don't affect each other (ignore the first three lines).
  Handle<Context> context = Context::New(NULL, global);

  // Store the context in the processor object in a persistent handle,
  // since we want the reference to remain after we return from this
  // method.
  context_ = Persistent<Context>::New(context);

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
  process_ = Persistent<Function>::New(process_fun);

  // All done; all went well
  return true;
}


bool JsHttpRequestProcessor::ExecuteScript
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

bool JsHttpRequestProcessor::InstallMaps
  ( map<string, string>* opts
  , map<string, string>* output
  ) 
{
  HandleScope handle_scope;

  // Wrap the map object in a JavaScript wrapper
  Handle<Object> opts_obj = WrapMap(opts);

  // Set the options object as a property on the global object.
  context_->Global()->Set(String::New("options"), opts_obj);

  Handle<Object> output_obj = WrapMap(output);
  context_->Global()->Set(String::New("output"), output_obj);

  return true;
}


bool JsHttpRequestProcessor::Process
  ( HttpRequest* request
  ) 
{
  // Create a handle scope to keep the temporary object references.
  HandleScope handle_scope;

  // Enter this processor's context so all the remaining operations
  // take place there
  Context::Scope context_scope(context_);

  // Wrap the C++ request object in a JavaScript wrapper
  Handle<Object> request_obj = WrapRequest(request);

  // Set up an exception handler before calling the Process function
  TryCatch try_catch;

  // Invoke the process function, giving the global object as 'this'
  // and one argument, the request.
  const int argc = 1;
  Handle<Value> argv[argc] = { request_obj };
  Handle<Value> result = process_->Call(context_->Global(), argc, argv);
  if (result.IsEmpty()) {
    String::Utf8Value error(try_catch.Exception());
    Log(*error);
    return false;
  } else {
    return true;
  }
}


JsHttpRequestProcessor::~JsHttpRequestProcessor
  (
  ) 
{
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
Handle<Object> JsHttpRequestProcessor::WrapMap
  ( map<string, string>* obj
  ) 
{
  // Handle scope for temporary handles.
  HandleScope handle_scope;

  // Fetch the template for creating JavaScript map wrappers.
  // It only has to be created once, which we do on demand.
  if (request_template_.IsEmpty()) {
    Handle<ObjectTemplate> raw_template = MakeMapTemplate();
    map_template_ = Persistent<ObjectTemplate>::New(raw_template);
  }
  Handle<ObjectTemplate> templ = map_template_;

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
map<string, string>* JsHttpRequestProcessor::UnwrapMap
  ( Handle<Object> obj
  ) 
{
  Handle<External> field = Handle<External>::Cast(obj->GetInternalField(0));
  void* ptr = field->Value();
  return static_cast<map<string, string>*>(ptr);
}


// Convert a JavaScript string to a std::string.  To not bother too
// much with string encodings we just use ascii.
string ObjectToString
  ( Local<Value> value
  )
{
  String::Utf8Value utf8_value(value);
  return string(*utf8_value);
}


Handle<Value> JsHttpRequestProcessor::MapGet
  ( Local<String> name
  , const AccessorInfo& info
  ) 
{
  // Fetch the map wrapped by this object.
  map<string, string>* obj = UnwrapMap(info.Holder());

  // Convert the JavaScript string to a std::string.
  string key = ObjectToString(name);

  // Look up the value if it exists using the standard STL ideom.
  map<string, string>::iterator iter = obj->find(key);

  // If the key is not present return an empty handle as signal
  if (iter == obj->end()) return Handle<Value>();

  // Otherwise fetch the value and wrap it in a JavaScript string
  const string& value = (*iter).second;
  return String::New(value.c_str(), value.length());
}


Handle<Value> JsHttpRequestProcessor::MapSet
  ( Local<String> name
  , Local<Value> value_obj
  , const AccessorInfo& info
  ) 
{
  // Fetch the map wrapped by this object.
  map<string, string>* obj = UnwrapMap(info.Holder());

  // Convert the key and value to std::strings.
  string key = ObjectToString(name);
  string value = ObjectToString(value_obj);

  // Update the map.
  (*obj)[key] = value;

  // Return the value; any non-empty handle will work.
  return value_obj;
}


Handle<ObjectTemplate> JsHttpRequestProcessor::MakeMapTemplate
  (
  )
{
  HandleScope handle_scope;

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
Handle<Object> JsHttpRequestProcessor::WrapRequest
  ( HttpRequest* request
  ) 
{
  // Handle scope for temporary handles.
  HandleScope handle_scope;

  // Fetch the template for creating JavaScript http request wrappers.
  // It only has to be created once, which we do on demand.
  if (request_template_.IsEmpty()) {
    Handle<ObjectTemplate> raw_template = MakeRequestTemplate();
    request_template_ = Persistent<ObjectTemplate>::New(raw_template);
  }
  Handle<ObjectTemplate> templ = request_template_;

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
HttpRequest* JsHttpRequestProcessor::UnwrapRequest
  ( Handle<Object> obj
  ) 
{
  Handle<External> field = Handle<External>::Cast(obj->GetInternalField(0));
  void* ptr = field->Value();
  return static_cast<HttpRequest*>(ptr);
}

Handle<Value> JsHttpRequestProcessor::GetPath
  ( Local<String> name
  , const AccessorInfo& info
  )
{
  HttpRequest* request = UnwrapRequest(info.Holder());
  const string& path = request->Path();
  return String::New(path.c_str(), path.length());
}

Handle<Value> JsHttpRequestProcessor::GetMethod
  ( Local<String> name
  , const AccessorInfo& info
  )
{
  HttpRequest* request = UnwrapRequest(info.Holder());
  // TODO allocate these strings only once. reference global
  switch(request->parser_info.method) {
    case ebb_request::EBB_COPY:      return String::New("COPY");
    case ebb_request::EBB_DELETE:    return String::New("DELETE");
    case ebb_request::EBB_GET:       return String::New("GET");
    case ebb_request::EBB_HEAD:      return String::New("HEAD");
    case ebb_request::EBB_LOCK:      return String::New("LOCK");
    case ebb_request::EBB_MKCOL:     return String::New("MKCOL");
    case ebb_request::EBB_MOVE:      return String::New("MOVE");
    case ebb_request::EBB_OPTIONS:   return String::New("OPTIONS");
    case ebb_request::EBB_POST:      return String::New("POST");
    case ebb_request::EBB_PROPFIND:  return String::New("PROPFIND");
    case ebb_request::EBB_PROPPATCH: return String::New("PROPPATCH");
    case ebb_request::EBB_PUT:       return String::New("PUT");
    case ebb_request::EBB_TRACE:     return String::New("TRACE");
    case ebb_request::EBB_UNLOCK:    return String::New("UNLOCK");
    default: 
      return Null();
  }
}


Handle<ObjectTemplate> JsHttpRequestProcessor::MakeRequestTemplate
  (
  ) 
{
  HandleScope handle_scope;

  Handle<ObjectTemplate> result = ObjectTemplate::New();
  result->SetInternalFieldCount(1);

  // Add accessors for each of the fields of the request.
  result->SetAccessor(String::NewSymbol("path"), GetPath);
  result->SetAccessor(String::NewSymbol("method"), GetMethod);

  // Again, return the result through the current handle scope.
  return handle_scope.Close(result);
}


// --- Test ---


void HttpRequestProcessor::Log
  ( const char* event
  ) 
{
  printf("Logged: %s\n", event);
}

void PrintMap
  ( map<string, string>* m
  ) 
{
  for (map<string, string>::iterator i = m->begin(); i != m->end(); i++) {
    pair<string, string> entry = *i;
    printf("%s: %s\n", entry.first.c_str(), entry.second.c_str());
  }
}

#if 0
int main(int argc, char* argv[]) {
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
  JsHttpRequestProcessor processor(source);
  map<string, string> output;
  if (!processor.Initialize(&options, &output)) {
    fprintf(stderr, "Error initializing processor.\n");
    return 1;
  }
  if (!ProcessEntries(&processor, kSampleSize, kSampleRequests))
    return 1;
  PrintMap(&output);
}
#endif
