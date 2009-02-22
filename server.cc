extern "C" {
#include <oi.h>
#include <ebb_request_parser.h>
}

#include "http_request.h"
#include "js_http_request_processor.h"

#include <stdio.h>
#include <assert.h>
#include <string>
#include <map>

#include <v8.h>

using namespace v8;
using namespace std;

#define PORT "1981"
#define MAX_URL 500 

static oi_server server;
static struct ev_loop *loop;
static JsHttpRequestProcessor *processor;

void on_path
  ( ebb_request *req
  , const char *buf
  , size_t len
  )
{
  HttpRequest *request = static_cast<HttpRequest*> (req->data);
  request->path.append(buf, len);
}

void on_headers_complete
  ( ebb_request *req
  )
{
  HttpRequest *request = static_cast<HttpRequest*> (req->data);

  bool r = processor->Process(request);
  if(r) {
    printf("successful request\n");
  } else {
    printf("unsuccesful request\n");
  }

}

void on_request_complete
  ( ebb_request *req
  )
{
  HttpRequest *request = static_cast<HttpRequest*> (req->data);
}

ebb_request * on_request
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
  request->parser_info.on_body             = NULL;
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
                             , static_cast<const char*> (buf) // FIXME change ebb to use void*
                             , count
                             );
  /* TODO check for errors */
}

static void on_close 
  ( oi_socket *socket
  )
{
  Connection *connection = static_cast<Connection*> (socket->data);
  /* TODO free requests */
  free(connection);
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
Handle<String> ReadFile
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

void ParseOptions
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

  processor = new JsHttpRequestProcessor(source);
  map<string, string> output;
  if (!processor->Initialize(&options, &output)) {
    fprintf(stderr, "Error initializing processor.\n");
    return 1;
  }


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
  return 0;
}
