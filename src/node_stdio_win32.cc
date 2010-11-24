#include <node_stdio.h>
#include <platform_win32.h>
#include <v8.h>

using namespace v8;
namespace node {


NO_IMPL(void, Stdio::DisableRawMode, , int fd);
NO_IMPL(void, Stdio::Flush, , );
NO_IMPL(static Handle<Value>, OpenStdin, RET_V8UNDEFINED, const Arguments& args);
NO_IMPL(static Handle<Value>, IsStdinBlocking, RET_V8FALSE, const Arguments& args);
NO_IMPL(static Handle<Value>, SetRawMode, RET_V8TRUE, const Arguments& args);
NO_IMPL(static Handle<Value>, GetColumns, RET_V8INT(80), const Arguments& args);
NO_IMPL(static Handle<Value>, GetRows, RET_V8INT(25), const Arguments& args);
NO_IMPL(static Handle<Value>, IsATTY, RET_V8FALSE, const Arguments& args);


/*
 * STDERR should always be blocking & utf-8
 * TODO: check correctness
 */
static Handle<Value>
WriteError (const Arguments& args)
{
  HandleScope scope;

  if (args.Length() < 1)
    return Undefined();

  String::Utf8Value msg(args[0]->ToString());

  fprintf(stderr, "%s", (char*)*msg);

  return Undefined();
}


/*
 * Assume that stdout is never blocking on windows
 * TODO: check correctness and really implement this
 */
static Handle<Value>
IsStdoutBlocking (const Arguments& args)
{
  return True();
}


void Stdio::Initialize(v8::Handle<v8::Object> target) {
  target->Set(String::NewSymbol("stdoutFD"), Integer::New(STDOUT_FILENO));
  target->Set(String::NewSymbol("stderrFD"), Integer::New(STDERR_FILENO));
  target->Set(String::NewSymbol("stdinFD"), Integer::New(STDIN_FILENO));

  NODE_SET_METHOD(target, "writeError", WriteError);
  NODE_SET_METHOD(target, "openStdin", OpenStdin);
  NODE_SET_METHOD(target, "isStdoutBlocking", IsStdoutBlocking);
  NODE_SET_METHOD(target, "isStdinBlocking", IsStdinBlocking);
  NODE_SET_METHOD(target, "setRawMode", SetRawMode);
  NODE_SET_METHOD(target, "getColumns", GetColumns);
  NODE_SET_METHOD(target, "getRows", GetRows);
  NODE_SET_METHOD(target, "isatty", IsATTY);
}


} // namespace node

NODE_MODULE(node_stdio, node::Stdio::Initialize);
