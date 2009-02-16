#ifndef js_http_request_processor_h
#define js_http_request_processor_h

#include <v8.h>
#include <string>
#include <map>

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
  explicit JsHttpRequestProcessor(Handle<String> script) : script_(script) { }
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
  static Handle<ObjectTemplate> MakeRequestTemplate();
  static Handle<ObjectTemplate> MakeMapTemplate();

  // Callbacks that access the individual fields of request objects.
  static Handle<Value> GetPath(Local<String> name, const AccessorInfo& info);
  static Handle<Value> GetReferrer(Local<String> name,
                                   const AccessorInfo& info);
  static Handle<Value> GetHost(Local<String> name, const AccessorInfo& info);
  static Handle<Value> GetUserAgent(Local<String> name,
                                    const AccessorInfo& info);

  // Callbacks that access maps
  static Handle<Value> MapGet(Local<String> name, const AccessorInfo& info);
  static Handle<Value> MapSet(Local<String> name,
                              Local<Value> value,
                              const AccessorInfo& info);

  // Utility methods for wrapping C++ objects as JavaScript objects,
  // and going back again.
  static Handle<Object> WrapMap(map<string, string>* obj);
  static map<string, string>* UnwrapMap(Handle<Object> obj);
  static Handle<Object> WrapRequest(HttpRequest* obj);
  static HttpRequest* UnwrapRequest(Handle<Object> obj);

  Handle<String> script_;
  Persistent<Context> context_;
  Persistent<Function> process_;
  static Persistent<ObjectTemplate> request_template_;
  static Persistent<ObjectTemplate> map_template_;
};

#endif // js_http_request_processor_h
