#include "node.h"
#include "http.h"
#include <http_parser.h>

#include <assert.h>
#include <stdio.h>

using namespace v8;
using namespace node;
using namespace std;

void
HTTPClient::Initialize (Handle<Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(HTTPClient::v8New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  target->Set(String::NewSymbol("HTTPClient"), t->GetFunction());

  NODE_SET_METHOD(t->InstanceTemplate(), "connect", Connection::v8Connect);
  NODE_SET_METHOD(t->InstanceTemplate(), "close", Connection::v8Close);
  NODE_SET_METHOD(t->InstanceTemplate(), "send", Connection::v8Send);
  NODE_SET_METHOD(t->InstanceTemplate(), "sendEOF", Connection::v8SendEOF);
}

Handle<Value>
HTTPClient::v8New (const Arguments& args)
{
  HandleScope scope;

  if (args[0]->IsFunction() == false)
    return ThrowException(String::New("Must pass a class as the first argument."));

  Handle<Function> protocol = Handle<Function>::Cast(args[0]);

  int argc = args.Length();
  Handle<Value> argv[argc];
  
  argv[0] = args.This();
  for (int i = 1; i < args.Length(); i++) {
    argv[i] = args[i];
  }

  Local<Object> protocol_instance = protocol->NewInstance(argc, argv);

  new HTTPClient(args.This(), protocol_instance);

  return args.This();
}

void
HTTPClient::OnReceive (const void *buf, size_t len)
{
  printf("http client got data!\n");
}

HTTPClient::HTTPClient (Handle<Object> handle, Handle<Object> protocol) 
  : Connection(handle, protocol) 
{
} 
