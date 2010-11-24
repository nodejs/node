#include <node.h>
#include <node_script.h>
#include <assert.h>



using namespace v8;
using namespace node;


Persistent<FunctionTemplate> node::Context::constructor_template;


void node::Context::Initialize (Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(node::Context::New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("Context"));

  target->Set(String::NewSymbol("Context"), constructor_template->GetFunction());
}


Handle<Value> node::Context::New (const Arguments& args) {
  HandleScope scope;

  node::Context *t = new node::Context();
  t->Wrap(args.This());

  return args.This();
}


node::Context::Context() : ObjectWrap() {
  context_ = v8::Context::New();
}


node::Context::~Context() {
  context_.Dispose();
}


Local<Object> node::Context::NewInstance() {
  Local<Object> context = constructor_template->GetFunction()->NewInstance();
  return context;
}


v8::Persistent<v8::Context> node::Context::GetV8Context() {
	return context_;
}


Persistent<FunctionTemplate> node::Script::constructor_template;


void node::Script::Initialize (Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(node::Script::New);
  constructor_template = Persistent<FunctionTemplate>::New(t);
  constructor_template->InstanceTemplate()->SetInternalFieldCount(1);
  constructor_template->SetClassName(String::NewSymbol("Script"));

  NODE_SET_PROTOTYPE_METHOD(constructor_template, "createContext", node::Script::CreateContext);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "runInContext", node::Script::RunInContext);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "runInThisContext", node::Script::RunInThisContext);
  NODE_SET_PROTOTYPE_METHOD(constructor_template, "runInNewContext", node::Script::RunInNewContext);
  NODE_SET_METHOD(constructor_template, "createContext", node::Script::CreateContext);
  NODE_SET_METHOD(constructor_template, "runInContext", node::Script::CompileRunInContext);
  NODE_SET_METHOD(constructor_template, "runInThisContext", node::Script::CompileRunInThisContext);
  NODE_SET_METHOD(constructor_template, "runInNewContext", node::Script::CompileRunInNewContext);

  target->Set(String::NewSymbol("Script"), constructor_template->GetFunction());
}


Handle<Value> node::Script::New (const Arguments& args) {
  if (!args.IsConstructCall()) {
    return FromConstructorTemplate(constructor_template, args);
  }

  HandleScope scope;

  node::Script *t = new node::Script();
  t->Wrap(args.Holder());

  return
    node::Script::EvalMachine<compileCode, thisContext, wrapExternal>(args);
}


node::Script::~Script() {
  script_.Dispose();
}


Handle<Value> node::Script::CreateContext (const Arguments& args) {
  HandleScope scope;

  Local<v8::Object> context = node::Context::NewInstance();

  if (args.Length() > 0) {

    Local<Object> sandbox = args[0]->ToObject();
    Local<Array> keys = sandbox->GetPropertyNames();

    for (uint32_t i = 0; i < keys->Length(); i++) {
      Handle<String> key = keys->Get(Integer::New(i))->ToString();
      Handle<Value> value = sandbox->Get(key);
      context->Set(key, value);
    }
  }


  return scope.Close(context);
}


Handle<Value> node::Script::RunInContext (const Arguments& args) {
  return
    node::Script::EvalMachine<unwrapExternal, userContext, returnResult>(args);
}


Handle<Value> node::Script::RunInThisContext (const Arguments& args) {
  return
    node::Script::EvalMachine<unwrapExternal, thisContext, returnResult>(args);
}


Handle<Value> node::Script::RunInNewContext(const Arguments& args) {
  return
    node::Script::EvalMachine<unwrapExternal, newContext, returnResult>(args);
}


Handle<Value> node::Script::CompileRunInContext (const Arguments& args) {
  return
    node::Script::EvalMachine<compileCode, userContext, returnResult>(args);
}


Handle<Value> node::Script::CompileRunInThisContext (const Arguments& args) {
  return
    node::Script::EvalMachine<compileCode, thisContext, returnResult>(args);
}


Handle<Value> node::Script::CompileRunInNewContext(const Arguments& args) {
  return
    node::Script::EvalMachine<compileCode, newContext, returnResult>(args);
}


template <node::Script::EvalInputFlags iFlag,
          node::Script::EvalContextFlags cFlag,
          node::Script::EvalOutputFlags oFlag>
    Handle<Value> node::Script::EvalMachine(const Arguments& args) {

  HandleScope scope;

  if (iFlag == compileCode && args.Length() < 1) {
    return ThrowException(Exception::TypeError(
          String::New("needs at least 'code' argument.")));
  }

  const int sbIndex = iFlag == compileCode ? 1 : 0;
  if (cFlag == userContext && args.Length() < (sbIndex + 1)) {
    return ThrowException(Exception::TypeError(
          String::New("needs a 'context' argument.")));
  }


  Local<String> code;
  if (iFlag == compileCode) code = args[0]->ToString();

  Local<Object> sandbox;
  if (cFlag == newContext) {
    sandbox = args[sbIndex]->IsObject() ? args[sbIndex]->ToObject() : Object::New();
  } else if (cFlag == userContext) {
    sandbox = args[sbIndex]->ToObject();
  }

  const int fnIndex = sbIndex + (cFlag == newContext ? 1 : 0);
  Local<String> filename = args.Length() > fnIndex
                           ? args[fnIndex]->ToString()
                           : String::New("evalmachine.<anonymous>");

  Persistent<v8::Context> context;

  Local<Array> keys;
  unsigned int i;
  if (cFlag == newContext) {
    // Create the new context
    context = v8::Context::New();

  } else if (cFlag == userContext) {
    // Use the passed in context
    Local<Object> contextArg = args[sbIndex]->ToObject();
    node::Context *nContext = ObjectWrap::Unwrap<node::Context>(sandbox);
    context = nContext->GetV8Context();
  }

  // New and user context share code. DRY it up.
  if (cFlag == userContext || cFlag == newContext) {
    // Enter the context
    context->Enter();

    // Copy everything from the passed in sandbox (either the persistent
    // context for runInContext(), or the sandbox arg to runInNewContext()).
    keys = sandbox->GetPropertyNames();

    for (i = 0; i < keys->Length(); i++) {
      Handle<String> key = keys->Get(Integer::New(i))->ToString();
      Handle<Value> value = sandbox->Get(key);
      if (value == sandbox) { value = context->Global(); }
      context->Global()->Set(key, value);
    }
  }

  // Catch errors
  TryCatch try_catch;

  Handle<Value> result;
  Handle<v8::Script> script;

  if (iFlag == compileCode) {
    // well, here node::Script::New would suffice in all cases, but maybe
    // Compile has a little better performance where possible
    script = oFlag == returnResult ? v8::Script::Compile(code, filename)
                                   : v8::Script::New(code, filename);
    if (script.IsEmpty()) {
      // FIXME UGLY HACK TO DISPLAY SYNTAX ERRORS.
      DisplayExceptionLine(try_catch);

      // Hack because I can't get a proper stacktrace on SyntaxError
      return try_catch.ReThrow();
    }
  } else {
    node::Script *nScript = ObjectWrap::Unwrap<node::Script>(args.Holder());
    if (!nScript) {
      return ThrowException(Exception::Error(
            String::New("Must be called as a method of Script.")));
    } else if (nScript->script_.IsEmpty()) {
      return ThrowException(Exception::Error(
            String::New("'this' must be a result of previous new Script(code) call.")));
    }

    script = nScript->script_;
  }


  if (oFlag == returnResult) {
    result = script->Run();
    if (result.IsEmpty()) return try_catch.ReThrow();
  } else {
    node::Script *nScript = ObjectWrap::Unwrap<node::Script>(args.Holder());
    if (!nScript) {
      return ThrowException(Exception::Error(
            String::New("Must be called as a method of Script.")));
    }
    nScript->script_ = Persistent<v8::Script>::New(script);
    result = args.This();
  }

  if (cFlag == userContext || cFlag == newContext) {
    // success! copy changes back onto the sandbox object.
    keys = context->Global()->GetPropertyNames();
    for (i = 0; i < keys->Length(); i++) {
      Handle<String> key = keys->Get(Integer::New(i))->ToString();
      Handle<Value> value = context->Global()->Get(key);
      if (value == context->Global()) { value = sandbox; }
      sandbox->Set(key, value);
    }
  }

  if (cFlag == newContext) {
    // Clean up, clean up, everybody everywhere!
    context->DetachGlobal();
    context->Exit();
    context.Dispose();
  } else if (cFlag == userContext) {
    // Exit the passed in context.
    context->Exit();
  }

  return result == args.This() ? result : scope.Close(result);
}

void node::InitEvals(Handle<Object> target) {
  HandleScope scope;

  node::Context::Initialize(target);
  node::Script::Initialize(target);
}

NODE_MODULE(node_evals, node::InitEvals);
