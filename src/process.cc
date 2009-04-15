#include "process.h"
#include "node.h"
#include <v8.h>
#include <stdlib.h>

using namespace v8;

static Handle<Value>
ExitCallback (const Arguments& args)
{
  int exit_code = 0;
  if (args.Length() > 0) exit_code = args[0]->IntegerValue();
  exit(exit_code);
  return Undefined(); 
}

static Handle<Value>
OnCallback (const Arguments& args)
{
  return Undefined(); 
}

void
NodeInit_process (Handle<Object> target)
{
  HandleScope scope;

  Local<Object> process = ObjectTemplate::New()->NewInstance();

  target->Set(String::NewSymbol("process"), process);

  // process.exit()
  Local<FunctionTemplate> process_exit = FunctionTemplate::New(ExitCallback);
  process->Set(String::NewSymbol("exit"), process_exit->GetFunction());

  // process.on()
  Local<FunctionTemplate> process_on = FunctionTemplate::New(OnCallback);
  process->Set(String::NewSymbol("on"), process_exit->GetFunction());
}
