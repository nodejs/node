#include <node.h>
#include <node_script.h>
#include <assert.h>

#include <v8-debug.h>

using namespace v8;
using namespace node;

Persistent<FunctionTemplate> node::Script::constructor_template;

void
node::Script::Initialize (Handle<Object> target)
{
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(node::Script::New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("Script"));

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "runInThisContext", node::Script::RunInThisContext);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "runInNewContext", node::Script::RunInNewContext);
  NODE_SET_METHOD(constructor_template, "runInThisContext", node::Script::CompileRunInThisContext);
  NODE_SET_METHOD(constructor_template, "runInNewContext", node::Script::CompileRunInNewContext);

  target->Set(String::NewSymbol("Script"), constructor_template->GetFunction());
}

Handle<Value>
node::Script::New (const Arguments& args)
{
  HandleScope scope;

  node::Script *t = new node::Script();
  t->Wrap(args.Holder());

  return
    node::Script::EvalMachine<compileCode, thisContext, wrapExternal>(args);
}

node::Script::~Script() {
  _script.Dispose();
}


Handle<Value>
node::Script::RunInThisContext (const Arguments& args)
{
  return
    node::Script::EvalMachine<unwrapExternal, thisContext, returnResult>(args);
}


Handle<Value>
node::Script::RunInNewContext(const Arguments& args) {
  return
    node::Script::EvalMachine<unwrapExternal, newContext, returnResult>(args);
}


Handle<Value>
node::Script::CompileRunInThisContext (const Arguments& args)
{
  return
    node::Script::EvalMachine<compileCode, thisContext, returnResult>(args);
}


Handle<Value>
node::Script::CompileRunInNewContext(const Arguments& args) {
  return
    node::Script::EvalMachine<compileCode, newContext, returnResult>(args);
}


// Extracts a C str from a V8 Utf8Value.
const char* ToCString(const v8::String::Utf8Value& value) {
  return *value ? *value : "<str conversion failed>";
}


static void ReportException(TryCatch &try_catch, bool show_line = false, bool show_rest = true) {
  Handle<Message> message = try_catch.Message();

  Handle<Value> error = try_catch.Exception();
  Handle<String> stack;

  if (error->IsObject()) {
    Handle<Object> obj = Handle<Object>::Cast(error);
    Handle<Value> raw_stack = obj->Get(String::New("stack"));
    if (raw_stack->IsString()) stack = Handle<String>::Cast(raw_stack);
  }

  if (show_line && !message.IsEmpty()) {
    // Print (filename):(line number): (message).
    String::Utf8Value filename(message->GetScriptResourceName());
    const char* filename_string = ToCString(filename);
    int linenum = message->GetLineNumber();
    fprintf(stderr, "%s:%i\n", filename_string, linenum);
    // Print line of source code.
    String::Utf8Value sourceline(message->GetSourceLine());
    const char* sourceline_string = ToCString(sourceline);
    fprintf(stderr, "%s\n", sourceline_string);
    // Print wavy underline (GetUnderline is deprecated).
    int start = message->GetStartColumn();
    for (int i = 0; i < start; i++) {
      fprintf(stderr, " ");
    }
    int end = message->GetEndColumn();
    for (int i = start; i < end; i++) {
      fprintf(stderr, "^");
    }
    fprintf(stderr, "\n");
  }

  if (show_rest) {
    if (stack.IsEmpty()) {
      message->PrintCurrentStackTrace(stderr);
    } else {
      String::Utf8Value trace(stack);
      fprintf(stderr, "%s\n", *trace);
    }
  }
  fflush(stderr);
}


template <node::Script::EvalInputFlags iFlag,
  node::Script::EvalContextFlags cFlag,
  node::Script::EvalOutputFlags oFlag>
Handle<Value> node::Script::EvalMachine(const Arguments& args) {
  HandleScope scope;

  if (iFlag == compileCode && args.Length() < 1) {
    return ThrowException(Exception::TypeError(
      String::New("needs at least 'code' argument.")
    ));
  }

  Local<String> code;
  if (iFlag == compileCode) { code = args[0]->ToString(); }

  Local<Object> sandbox;
  const int sbIndex = iFlag == compileCode ? 1 : 0;
  if (cFlag == newContext) {
    sandbox = args.Length() > sbIndex ? args[sbIndex]->ToObject() : Object::New();
  }
  const int fnIndex = sbIndex + (cFlag == newContext ? 1 : 0);
  Local<String> filename = args.Length() > fnIndex ? args[fnIndex]->ToString()
                                             : String::New("evalmachine.<anonymous>");

  Persistent<Context> context;
  Local<Array> keys;
  unsigned int i;
  if (cFlag == newContext) {
    // Create the new context
    context = Context::New();

    // Enter and compile script
    context->Enter();

    // Copy objects from global context, to our brand new context
    keys = sandbox->GetPropertyNames();

    for (i = 0; i < keys->Length(); i++) {
      Handle<String> key = keys->Get(Integer::New(i))->ToString();
      Handle<Value> value = sandbox->Get(key);
      context->Global()->Set(key, value);
    }
  }

  // Catch errors
  TryCatch try_catch;

  Handle<Value> result;
  Handle<v8::Script> script;

  if (iFlag == compileCode) {
    // well, here node::Script::New would suffice in all cases, but maybe Compile has a little better performance where possible
    script = oFlag == returnResult ? v8::Script::Compile(code, filename) : v8::Script::New(code, filename);
    if (script.IsEmpty()) {
      // Hack because I can't get a proper stacktrace on SyntaxError
      ReportException(try_catch, true, false);
      result = ThrowException(try_catch.Exception());
    }
  } else {
    node::Script *nScript = ObjectWrap::Unwrap<node::Script>(args.Holder());
    if (!nScript) {
      Local<Value> exception =
        Exception::Error(String::New("Must be called as a method of Script."));
      result = ThrowException(exception);
    } else if (nScript->_script.IsEmpty()) {
      Local<Value> exception =
        Exception::Error(String::New("'this' must be a result of previous new Script(code) call."));
      result = ThrowException(exception);
    } else {
      script = nScript->_script;
    }
  }

  if (result.IsEmpty()) {
    if (oFlag == returnResult) {
      result = script->Run();
    } else {
      node::Script *nScript = ObjectWrap::Unwrap<node::Script>(args.Holder());
      if (!nScript) {
        Local<Value> exception =
          Exception::Error(String::New("Must be called as a method of Script."));
        result = ThrowException(exception);
      } else {
        nScript->_script = Persistent<v8::Script>::New(script);
        result = args.This();
      }
    }
    if (result.IsEmpty()) {
      result = ThrowException(try_catch.Exception());
    } else if (cFlag == newContext) {
      // success! copy changes back onto the sandbox object.
      keys = context->Global()->GetPropertyNames();
      for (i = 0; i < keys->Length(); i++) {
        Handle<String> key = keys->Get(Integer::New(i))->ToString();
        Handle<Value> value = context->Global()->Get(key);
        sandbox->Set(key, value);
      }
    }
  }

  if (cFlag == newContext) {
    // Clean up, clean up, everybody everywhere!
    context->DetachGlobal();
    context->Exit();
    context.Dispose();
  }

  return result == args.This() ? result : scope.Close(result);
}
