#include <oi.h>
#include <ebb_request_parser.h>

#include <stdio.h>
#include <assert.h>
#include <string>
#include <map>

#include <v8.h>

using namespace v8;
using namespace std;

#define PORT "1981"

static oi_server server;
static struct ev_loop *loop;

static Persistent<Context> context_;
static Persistent<Function> process_;
static Persistent<ObjectTemplate> request_template_;

class Connection {
public:
  Connection ( void)
  {
    oi_socket_init (&socket, 30.0);
    ebb_request_parser_init (&parser);
  }
  ebb_request_parser parser;
  oi_socket socket;
};

class HttpRequest {
 public:
  HttpRequest (Connection &c);
  ~HttpRequest();

  string path;

  Connection &connection;
  ebb_request parser_info;

  Persistent<Object> js_object;
};

static HttpRequest* UnwrapRequest
  ( Handle<Object> obj
  ) 
{
  Handle<External> field = Handle<External>::Cast(obj->GetInternalField(0));
  void* ptr = field->Value();
  return static_cast<HttpRequest*>(ptr);
}

static void make_onBody_callback
  ( HttpRequest *request
  , const char *base
  , size_t length
  )
{
  HandleScope handle_scope;


  Handle<Object> obj = request->js_object;
  // XXX don't always allocate onBody strings
  Handle<Value> onBody_val = request->js_object->Get(String::NewSymbol("onBody"));  
  if (!onBody_val->IsFunction()) return;
  Handle<Function> onBody = Handle<Function>::Cast(onBody_val);

  TryCatch try_catch;
  const int argc = 1;
  Handle<Value> argv[argc];
  
  if(length) {
    Handle<String> chunk = String::New(base, length);
    argv[0] = chunk;
  } else {
    argv[0] = Null();
  }

  Handle<Value> result = onBody->Call(request->js_object, argc, argv);

  if (result.IsEmpty()) {
    String::Utf8Value error(try_catch.Exception());
    printf("error: %s\n", *error);
  }
}

static Handle<Value> GetMethodString
  ( int method 
  )
{
  switch(method) {
    case EBB_COPY:      return String::New("COPY");
    case EBB_DELETE:    return String::New("DELETE");
    case EBB_GET:       return String::New("GET");
    case EBB_HEAD:      return String::New("HEAD");
    case EBB_LOCK:      return String::New("LOCK");
    case EBB_MKCOL:     return String::New("MKCOL");
    case EBB_MOVE:      return String::New("MOVE");
    case EBB_OPTIONS:   return String::New("OPTIONS");
    case EBB_POST:      return String::New("POST");
    case EBB_PROPFIND:  return String::New("PROPFIND");
    case EBB_PROPPATCH: return String::New("PROPPATCH");
    case EBB_PUT:       return String::New("PUT");
    case EBB_TRACE:     return String::New("TRACE");
    case EBB_UNLOCK:    return String::New("UNLOCK");
  }
  return Null();
}

static Handle<Value> RespondCallback
  ( const Arguments& args
  ) 
{
  HttpRequest* request = UnwrapRequest(args.Holder());
  HandleScope scope;
  Handle<Value> arg = args[0];

  // TODO Make sure that we write reponses in the correct order. With
  // keep-alive it's possible that one response can return before the last
  // one has been sent!!!
  
  //printf("response called\n");

  if(arg == Null()) {

    //printf("response got null\n");
    delete request; 

  } else {

    Handle<String> s = arg->ToString();

    //printf("response called len %d\n", s->Length());

    oi_buf *buf = oi_buf_new2(s->Length());
    s->WriteAscii(buf->base, 0, s->Length());

    oi_socket_write(&request->connection.socket, buf);

  }

  return Undefined();
}

HttpRequest::~HttpRequest ()
{
  //printf("request is being destructed\n");

  connection.socket.on_drain = oi_socket_close;

  HandleScope scope;
  // delete a reference to the respond method
  //js_object->Delete(String::New("respond"));
  js_object.Dispose();
}

HttpRequest::HttpRequest (Connection &c) : connection(c)
{
  ebb_request_init(&parser_info); 
}

static void on_path
  ( ebb_request *req
  , const char *buf
  , size_t len
  )
{
  HttpRequest *request = static_cast<HttpRequest*> (req->data);
  request->path.append(buf, len);
}

static void on_headers_complete
  ( ebb_request *req
  )
{
  HttpRequest *request = static_cast<HttpRequest*> (req->data);

  HandleScope handle_scope;

  if (request_template_.IsEmpty()) {
    Handle<ObjectTemplate> raw_template = ObjectTemplate::New();
    raw_template->SetInternalFieldCount(1);
    raw_template->Set(String::New("respond"), FunctionTemplate::New(RespondCallback));

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

  result->Set ( String::NewSymbol("path")
              , String::New(request->path.c_str(), request->path.length())
              );

  result->Set ( String::NewSymbol("method")
              , GetMethodString(request->parser_info.method)
              );

  request->js_object = Persistent<Object>::New(result);

  // Enter this processor's context so all the remaining operations
  // take place there
  Context::Scope context_scope(context_);

  // Set up an exception handler before calling the Process function
  TryCatch try_catch;

  // Invoke the process function, giving the global object as 'this'
  // and one argument, the request.
  const int argc = 1;
  Handle<Value> argv[argc] = { request->js_object };
  Handle<Value> r = process_->Call(context_->Global(), argc, argv);
  if (r.IsEmpty()) {
    String::Utf8Value error(try_catch.Exception());
    printf("error: %s\n", *error);
  }
}

static void on_request_complete
  ( ebb_request *req
  )
{
  HttpRequest *request = static_cast<HttpRequest*> (req->data);

  make_onBody_callback(request, NULL, 0); // EOF
}


static void on_body
  ( ebb_request *req
  , const char *base
  , size_t length
  )
{
  //printf("on body %d\n", length);

  HttpRequest *request = static_cast<HttpRequest*> (req->data);

  if(length)
    make_onBody_callback(request, base, length);
}

static ebb_request * on_request
  ( void *data
  ) 
{
  Connection *connection = static_cast<Connection*> (data);

  HttpRequest *request = new HttpRequest(*connection);
  
  request->parser_info.on_path             = on_path;
  request->parser_info.on_query_string     = NULL;
  request->parser_info.on_uri              = NULL;
  request->parser_info.on_fragment         = NULL;
  request->parser_info.on_header_field     = NULL;
  request->parser_info.on_header_value     = NULL;
  request->parser_info.on_headers_complete = on_headers_complete;
  request->parser_info.on_body             = on_body;
  request->parser_info.on_complete         = on_request_complete;
  request->parser_info.data                = request;

  return &request->parser_info;
}

static void on_read 
  ( oi_socket *socket
  , const void *buf
  , size_t count
  )
{
  Connection *connection = static_cast<Connection*> (socket->data);
  ebb_request_parser_execute ( &connection->parser
                             // FIXME change ebb to use void*
                             , static_cast<const char*> (buf) 
                             , count
                             );
  if(ebb_request_parser_has_error(&connection->parser)) {
    fprintf(stderr, "parse error closing connection\n");
    oi_socket_close(&connection->socket);
  }
}

static void on_close 
  ( oi_socket *socket
  )
{
  Connection *connection = static_cast<Connection*> (socket->data);
  // TODO free requests
  delete connection;
}

static void on_drain 
  ( oi_socket *socket
  )
{
  Connection *connection = static_cast<Connection*> (socket->data);
  //oi_socket_close(&connection->socket);
}

static oi_socket* new_connection 
  ( oi_server *server
  , struct sockaddr *addr
  , socklen_t len
  )
{
  Connection *connection = new Connection();
    connection->socket.on_read    = on_read;
    connection->socket.on_error   = NULL;
    connection->socket.on_close   = on_close;
    connection->socket.on_timeout = NULL;
    connection->socket.on_drain   = on_drain;
    connection->socket.data       = connection;

    connection->parser.new_request = on_request;
    connection->parser.data        = connection;


  return &connection->socket;
}


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
    String::Utf8Value error(try_catch.Exception());
    printf("error: %s\n", *error);
    // The script failed to compile; bail out.
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

int main 
  ( int argc
  , char *argv[]
  ) 
{
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

  Handle<ObjectTemplate> global = ObjectTemplate::New();
  global->Set(String::New("log"), FunctionTemplate::New(LogCallback));

  Handle<Context> context = Context::New(NULL, global);

  context_ = Persistent<Context>::New(context);

  Context::Scope context_scope(context);

  // Compile and run the script
  if (!compile(source))
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

  /////////////////////////////////////
  /////////////////////////////////////
  /////////////////////////////////////

  loop = ev_default_loop(0);
  
  oi_server_init(&server, 1024);
  server.on_connection = new_connection;

  // get addrinfo for localhost, PORT
  struct addrinfo *servinfo;
  struct addrinfo hints;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  int r = getaddrinfo(NULL, PORT, &hints, &servinfo);
  assert(r == 0);

  r = oi_server_listen(&server, servinfo);
  assert(r == 0);

  oi_server_attach(&server, loop);

  printf("Running at http://localhost:%s/\n", PORT);

  ev_loop(loop, 0);

  context_.Dispose();
  process_.Dispose();

  return 0;
}
