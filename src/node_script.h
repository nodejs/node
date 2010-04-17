#ifndef node_script_h
#define node_script_h

#include <node.h>
#include <node_object_wrap.h>
#include <v8.h>
#include <ev.h>

namespace node {

class Script : ObjectWrap {
 public:
  static void Initialize (v8::Handle<v8::Object> target);

  enum EvalInputFlags { compileCode, unwrapExternal };
  enum EvalContextFlags { thisContext, newContext };
  enum EvalOutputFlags { returnResult, wrapExternal };

  template <EvalInputFlags iFlag, EvalContextFlags cFlag, EvalOutputFlags oFlag>
  static v8::Handle<v8::Value> EvalMachine(const v8::Arguments& args);

 protected:
  static v8::Persistent<v8::FunctionTemplate> constructor_template;

  Script () : ObjectWrap () {}
  ~Script();

  static v8::Handle<v8::Value> New (const v8::Arguments& args);
  static v8::Handle<v8::Value> RunInThisContext (const v8::Arguments& args);
  static v8::Handle<v8::Value> RunInNewContext (const v8::Arguments& args);
  static v8::Handle<v8::Value> CompileRunInThisContext (const v8::Arguments& args);
  static v8::Handle<v8::Value> CompileRunInNewContext (const v8::Arguments& args);

  v8::Persistent<v8::Script> _script;
};

} // namespace node
#endif //  node_script_h
