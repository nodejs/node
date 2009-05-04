#ifndef node_http_h
#define node_http_h

#include <v8.h>
#include "net.h"
#include <http_parser.h>

namespace node {

class HTTPConnection : public Connection {
public:
  static void Initialize (v8::Handle<v8::Object> target);

protected:
  static v8::Persistent<v8::FunctionTemplate> constructor_template;
  static v8::Handle<v8::Value> v8NewClient (const v8::Arguments& args);
  static v8::Handle<v8::Value> v8NewServer (const v8::Arguments& args);

  HTTPConnection (v8::Handle<v8::Object> handle, 
                  v8::Handle<v8::Function> protocol_class,
                  enum http_parser_type type);

  void OnReceive (const void *buf, size_t len);

  static int on_message_begin (http_parser *parser);
  static int on_path (http_parser *parser, const char *at, size_t length);
  static int on_query_string (http_parser *parser, const char *at, size_t length);
  static int on_uri (http_parser *parser, const char *at, size_t length);
  static int on_fragment (http_parser *parser, const char *at, size_t length);
  static int on_header_field (http_parser *parser, const char *buf, size_t len);
  static int on_header_value (http_parser *parser, const char *buf, size_t len);
  static int on_headers_complete (http_parser *parser);
  static int on_body (http_parser *parser, const char *buf, size_t len);
  static int on_message_complete (http_parser *parser);

  http_parser parser_;
};

class HTTPServer : public Acceptor {
public:
  HTTPServer (v8::Handle<v8::Object> handle, v8::Handle<v8::Object> options);

protected:
  static v8::Handle<v8::Value> v8New (const v8::Arguments& args);
  Connection* OnConnection (struct sockaddr *addr, socklen_t len);
};

} // namespace node
#endif
