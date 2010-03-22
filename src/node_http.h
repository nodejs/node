#ifndef node_http_h
#define node_http_h

#include <node_net.h>
#include <v8.h>
#include <http_parser.h>

namespace node {

class HTTPConnection : public Connection {
public:
  static void Initialize (v8::Handle<v8::Object> target);

  static v8::Persistent<v8::FunctionTemplate> client_constructor_template;
  static v8::Persistent<v8::FunctionTemplate> server_constructor_template;

protected:
  static v8::Handle<v8::Value> NewClient (const v8::Arguments& args);
  static v8::Handle<v8::Value> NewServer (const v8::Arguments& args);
  static v8::Handle<v8::Value> ResetParser(const v8::Arguments& args);

  HTTPConnection (enum http_parser_type t)
    : Connection()
  {
    type_ = t;
    ResetParser();
  }

  void ResetParser() {
    http_parser_init (&parser_, type_);
    parser_.data = this;
  }

  void OnReceive (const void *buf, size_t len);
  void OnEOF ();

  static int on_message_begin (http_parser *parser);
  static int on_url (http_parser *parser, const char *at, size_t length);
  static int on_query_string (http_parser *parser, const char *at, size_t length);
  static int on_path (http_parser *parser, const char *at, size_t length);
  static int on_fragment (http_parser *parser, const char *at, size_t length);
  static int on_header_field (http_parser *parser, const char *buf, size_t len);
  static int on_header_value (http_parser *parser, const char *buf, size_t len);
  static int on_headers_complete (http_parser *parser);
  static int on_body (http_parser *parser, const char *buf, size_t len);
  static int on_message_complete (http_parser *parser);

  enum http_parser_type type_;
  http_parser parser_;
  friend class HTTPServer;
};

class HTTPServer : public Server {
public:
  static void Initialize (v8::Handle<v8::Object> target);
  static v8::Persistent<v8::FunctionTemplate> constructor_template;

protected:
  static v8::Handle<v8::Value> New (const v8::Arguments& args);

  HTTPServer (void) : Server() {}

  v8::Handle<v8::FunctionTemplate> GetConnectionTemplate (void);
  Connection* UnwrapConnection (v8::Local<v8::Object> connection);
};

} // namespace node
#endif
