#ifndef http_request_h
#define http_request_h
extern "C" {
#include <oi_socket.h>
#include <ebb_request_parser.h>
}
#include <string>

using namespace std;

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
  HttpRequest (Connection &c) : connection(c)
  { 
    ebb_request_init(&parser_info); 
  }
  ~HttpRequest() { }

  string path;

  Connection &connection;
  ebb_request parser_info;
};

#endif // http_request_h
