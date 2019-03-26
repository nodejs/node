## V8 API usage for Node.js

v8 docs, not particularly useful:
- https://v8.dev/docs

Some API info at https://v8.dev/docs/embed

- An isolate is a VM instance with its own heap.
  Node has one isolate.
  Can get from `Local<Object>->GetIsolate()`, `context->GetIsolate()`

- `Local<...>`: A local handle is a pointer to an object. All V8 objects are
  accessed using handles, they are necessary because of the way the V8 garbage
  collector works.  Local handles can only be allocated on the stack, in a
  scope, not with new, and will be deleted as stack unwinds, usually with a
  scope.
  - XXX don't see us create scopes much in node, why?
  - XXX locals go into a HandleScope, for auto-dispose? Always, or by default?
- Persistent handles last past C++ functions.
  - PersistentBase::SetWeak trigger a callback from the garbage collector when
    the only references to an object are from weak persistent handles.
  - A UniquePersistent<SomeType> handle relies on C++ constructors and
    destructors to manage the lifetime of the underlying object.
  - A Persistent<SomeType> can be constructed with its constructor, but must be
    explicitly cleared with Persistent::Reset.
- Eternal is a persistent handle for JavaScript objects that are expected to
  never be deleted. It is cheaper to use because it relieves the garbage
  collector from determining the liveness of that object.
- A handle scope can be thought of as a container for any number of handles.
  When you've finished with your handles, instead of deleting each one
  individually you can simply delete their scope.
- EscapableHandleScope: Locals can't be returned, their scope will delete them,
  so you need an escable scope, and to `return scope.Escape(...the local)`. It
  scopes the locals into the enclosing scope and returns a local for that scope.

Local, MaybeLocal, Maybe, Value, oh my...

https://groups.google.com/forum/#!topic/v8-users/gQVpp1HmbqM

- MaybeLocal may be "empty", basically not contain a pointer of its type.  See
  `class MaybeLocal`, has some useful notes on why, but basically its returned
  by anything that might fail to have a value.
- If you know that the MaybeLocal has a value, then call ToLocalChecked() and
  node will abort with CHECK() if you are wrong.
- Otherwise, you have to call `bool ToLocal(Local<S>* out)`, and check the
  return value to see if there was a value. Or call IsEmpty() to check.

- Maybe is similar, but doesn't hold a Local, just a value of T. "Just" means it
  "just has a value", a bizarrely named Haskellism :-(. It has a To() and
  ToChecked() similar to MaybeLocal.
- A common node idiom is to make a seemingly side-effect free call to
  `.FromJust()` after `->Set()`, which will crash node if the Set failed.
  FromJust is also() commonly called  after getting a Maybe<> of a concrete data
  type from a Local<Value>. It will crash if the conversion fails!

        int32_t v = (Local<Value>)->In32Value(env->context()).FromJust()

- As<T> always returns a value, though probably not one that is useful, its
  usually best to check the type:
  - Boolean becomes 1/0 as int, "true"/"false" as strings, etc.
  - Numbers become false as Boolean (for any value), -3 casts to String "-3"
  - Functions become numerically zero, and "function () { const hello=0; }" as a
    String
  - Examples:
        CHECK(args[0]->IsInt32());  // Optional, but typical
        Local<Int32> l = args[0].As<Int32>(); int32_t v = li->Value();

        CHECK(args[0]->IsString());  // Optional, but typical
        const node::Utf8Value v(env->isolate(), args[0]); const char* s = *s;

- To<T> will convert values in fairly typical js way:
   ... never seems to be used by node?
   AFAICT, is identical to the As<> route, except for Boolean, which is always
   false with As<T>(), but is "expected" with ToT().

- FunctionCallbackInfo
  - can GetIsolate()
  - can get Environment: Environment* env = Environment::GetCurrent(args);
  - can get args using 0-based index
  - returns Local<Value>
  - has a .Length(), access past length returns a Local<Value> where value is
    Undefined
  - has a number of Is*() predicates which check exact type of Value, and
    (mostly) do NOT consider possible conversions:
    - {then: ()=>{}} is not considered a Promise,
    - `1` is not considered `true`,
    - `null` is not an `Object` (!),
    - new String() is a StringObject (not a String),
    - 3 is a Int32 and also a Uint32, -3 is only a Int32
    - etc.

Conventions on arg checking: two patterns are common:
1. C++ functions directly exposed to the user
2. C++ functions wrapped in a js function, only js is exposed to the user

The first option requires careful checking of argument types.

The second option is becoming more common. In this case the js function can
check all the argument types are as expected (throwing an error if they are
not), and destructure options properties to pass them as positional args (so the
C++ doesn't have to do Object property access and presence/type checks).  C++
can use its args fairly directly, aborting if the js layer failed to pass the
expected types:

    CHECK(args[0]->IsInt32());
    int32_t arg0 = args[0].As<Int32>()->Value();


- A context is an execution environment that allows separate, unrelated,
  JavaScript code to run in a single instance of V8. The motivation for using
  contexts in V8 was so that each window and iframe in a browser can have its
  own fresh JavaScript environment.
  XXX Node uses one context, mostly, does vm. create new ones? anything else?

  Can get from `isolate->GetCurrentContext()`


XXX Function vs FunctionTemplate ... wth?


- node::Environment contains an Isolate and a Context, various other
  information global to Node, and many convenient methods.
  
  It is possible to get an Environment from many v8 objects by calling
  Environment::GetCurrent() on a v8::Isolate*, v8::Local<v8::Context>,
  v8::FunctionCallbackInfo<v8::Value>, etc...

  An Environment can be used to get an Isolate, Context, uv_loop_t, etc.
 
  Commonly used convenience methods:
  - ThrowError/TypeError/RangeError/ErrnoException/...
  XXX why are some called Error and others called Exception?
  - SetMethod/SetMethodNoSideEffect/SetProtoMethod/SetTemplateMethod
  XXX difference?
  - SetImmediate
  - EnvironmentOptions* options(): "some" options...
  XXX how to reach PerIsolateOptions, PerProcessOptions
  - etc.

  Contains many global strings and symbols.
  XXX ...


## `NODE_MODULE_CONTEXT_AWARE_INTERNAL`

See: https://nodejs.org/api/addons.html#addons_context_aware_addons

Called with:
- `Local<Object> exports`: where to put exported properties, conventionally
  called `target` in node
- `Local<Value> module`: conventionally unused in node
XXX what is this for? addon docs don't mention or use it
- `Local<Context> context`:
- void* priv: not commonly used
XXX where is it ever used? for what?

Initialize is generally used to set methods and contants:

        Environment* env = Environment::GetCurrent(context);
        env->SetMethod(target, "name", Name);
        // ... see Environment for method creation convenience functions
        NODE_DEFINE_CONSTANT(target, MACRO_NAME);
        // read-only, not deletable, not enumerable:
        NODE_DEFINE_HIDDEN_CONSTANT(target, MACRO_NAME);

For functions that wrap a C++ object in a js object, manually do what SetMethod
does, see Hmac::Initialize as an example:
1. Create a FunctionTemplate, used to setup function properties. The various
wrappers all call v8::FunctionTemplate::New() with various arguments, but
env->NewFunctionTemplate() is most commonly used. Signature,
ConstructorBehavior, and SideEffectType can be customized, but aren't
documented, and usually are left as default.
2. Call env->setProtoMethod() to setup instance methods
3. Get a function from the template
4. Set a string in the target to the function

Mysterious boilerplate:
- ToLocalChecked() see MaybeLocal vs Local
- FromJust(): XXX

    
    
        Local<FunctionTemplate> t = env->NewFunctionTemplate(New);

Fields are used to store pointers to C++ objects:
XXX I think
        t->InstanceTemplate()->SetInternalFieldCount(1);
Set methods:


https://v8.dev/docs/embed#more-example-code
- XXX read through, it has examples of calling the API



- <http://izs.me/v8-docs/classv8_1_1Object.html>
- https://code.google.com/p/v8/
- Building: <https://code.google.com/p/v8/wiki/BuildingWithGYP>
- how to compile js to see what it looks like?
- [Breaking V8 Changes](https://docs.google.com/a/strongloop.com/document/d/1g8JFi8T_oAE_7uAri7Njtig7fKaPDfotU6huOa1alds/edit)
- <https://developers.google.com/v8/get_started>
- <https://chromium.googlesource.com/v8/v8/+/master/docs/using_git.md>

- <https://developers.google.com/v8/embed>

Handle is base, from that are Local (go in HandleScope), and Persistent
(manually managed scope). Constructors (String::New) seem to return Locals.

`return Local<Array>();` ... seems to do exactly what you are not supposed
to do... whats up?
