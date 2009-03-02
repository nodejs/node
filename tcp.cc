#include "tcp.h"

#include <oi_socket.h>
#include <oi_async.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>



/*
  Target API

  TCP.connect({

      host: "google.com",

      port: 80, 

      connect: function () {
          this.write("GET /search?q=hello HTTP/1.0\r\n\r\n");
      },

      read: function (data) {

          request.respond("<table> <td>" + data + "</td> </table>");

      },

      drain: function () {
      }, 

      error: function () {
      } 
  });

*/

static oi_async thread_pool;

static struct addrinfo tcp_hints = 
/* ai_flags      */ { AI_PASSIVE
/* ai_family     */ , AF_UNSPEC
/* ai_socktype   */ , SOCK_STREAM
/* ai_protocol   */ , 0
/* ai_addrlen    */ , 0
/* ai_addr       */ , 0
/* ai_canonname  */ , 0
/* ai_next       */ , 0
                    };

static struct ev_loop *loop;

class TCPClient {
public:
  oi_task resolve_task;
  oi_socket socket;
  struct addrinfo *address;
  Persistent<Object> options;
};

static void on_connect
  ( oi_socket *socket
  )
{
  TCPClient *client = static_cast<TCPClient*> (socket->data);

  HandleScope scope;

  Handle<Value> connect_value = client->options->Get( String::NewSymbol("connect") );
  if (!connect_value->IsFunction())
    return; // error!
  Handle<Function> connect_cb = Handle<Function>::Cast(connect_value);

  TryCatch try_catch;
  Handle<Value> r = connect_cb->Call(client->options, 0, NULL);
  if (r.IsEmpty()) {
    String::Utf8Value error(try_catch.Exception());
    printf("connect error: %s\n", *error);
  }
}

static void resolve_done
  ( oi_task *resolve_task
  , int result
  )
{
  TCPClient *client = static_cast<TCPClient*> (resolve_task->data);

  if(result != 0) {
    printf("error. TODO make call options error callback\n");
    client->options.Dispose();
    delete client;
    return;
  }

  // Got the address succesfully. Let's connect now.  

  oi_socket_init(&client->socket, 30.0); // TODO adjustable timeout

  client->socket.on_connect = on_connect;
  client->socket.on_read    = NULL;
  client->socket.on_drain   = NULL;
  client->socket.on_error   = NULL;
  client->socket.on_close   = NULL;
  client->socket.on_timeout = NULL;
  client->socket.data = client;

  oi_socket_connect (&client->socket, client->address);
  oi_socket_attach (&client->socket, loop);


  freeaddrinfo(client->address);
  client->address = NULL;
}

static Handle<Value> Connect
  ( const Arguments& args
  ) 
{
  if (args.Length() < 1)
    return Undefined();

  HandleScope scope;

  Handle<Value> arg = args[0];
  Handle<Object> options = arg->ToObject();

  /* Make sure the user has provided at least host and port */   

  Handle<Value> host_value = options->Get( String::NewSymbol("host") );

  if(host_value->IsUndefined()) 
    return False();    

  Handle<Value> port_value = options->Get( String::NewSymbol("port") );

  if(port_value->IsUndefined()) 
    return False();    

  Handle<String> host = host_value->ToString();
  Handle<String> port = port_value->ToString();

  char host_s[host->Length()+1];  // + 1 for \0
  char port_s[port->Length()+1];

  host->WriteAscii(host_s, 0, host->Length());
  port->WriteAscii(port_s, 0, port->Length());

  printf("resolving host: %s, port: %s\n", host_s, port_s);

  TCPClient *client = new TCPClient;

  oi_task_init_getaddrinfo ( &client->resolve_task
                           , resolve_done
                           , host_s
                           , port_s
                           , &tcp_hints
                           , &client->address
                           );
  client->options = Persistent<Object>::New(options); 

  oi_async_submit (&thread_pool, &client->resolve_task);
}

Handle<Object> tcp_initialize
  ( struct ev_loop *_loop
  )
{
  loop = _loop;

  oi_async_init(&thread_pool);
  oi_async_attach(loop, &thread_pool);

  HandleScope scope;

  Local<Object> t = Object::New();

  t->Set(String::New("connect"), FunctionTemplate::New(Connect)->GetFunction());

  return scope.Close(t);
}

