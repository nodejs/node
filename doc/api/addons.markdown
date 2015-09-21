# Addons

Addons are dynamically linked shared objects. They can provide glue to C and
C++ libraries. The API (at the moment) is rather complex, involving
knowledge of several libraries:

 - V8 JavaScript, a C++ library. Used for interfacing with JavaScript:
   creating objects, calling functions, etc.  Documented mostly in the
   `v8.h` header file (`deps/v8/include/v8.h` in the Node.js source
   tree), which is also available
   [online](https://v8docs.nodesource.com/).

 - [libuv](https://github.com/libuv/libuv), C event loop library.
   Anytime one needs to wait for a file descriptor to become readable,
   wait for a timer, or wait for a signal to be received one will need
   to interface with libuv. That is, if you perform any I/O, libuv will
   need to be used.

 - Internal Node.js libraries. Most importantly is the `node::ObjectWrap`
   class which you will likely want to derive from.

 - Others. Look in `deps/` for what else is available.

Node.js statically compiles all its dependencies into the executable.
When compiling your module, you don't need to worry about linking to
any of these libraries.

All of the following examples are available for
[download](https://github.com/rvagg/node-addon-examples) and may be
used as a starting-point for your own Addon.

## Hello world

To get started let's make a small Addon which is the C++ equivalent of
the following JavaScript code:

    module.exports.hello = function() { return 'world'; };

First we create a file `hello.cc`:

    // hello.cc
    #include <node.h>

    namespace demo {

    using v8::FunctionCallbackInfo;
    using v8::Isolate;
    using v8::Local;
    using v8::Object;
    using v8::String;
    using v8::Value;

    void Method(const FunctionCallbackInfo<Value>& args) {
      Isolate* isolate = args.GetIsolate();
      args.GetReturnValue().Set(String::NewFromUtf8(isolate, "world"));
    }

    void init(Local<Object> exports) {
      NODE_SET_METHOD(exports, "hello", Method);
    }

    NODE_MODULE(addon, init)

    }  // namespace demo

Note that all Node.js addons must export an initialization function:

    void Initialize(Local<Object> exports);
    NODE_MODULE(module_name, Initialize)

There is no semi-colon after `NODE_MODULE` as it's not a function (see
`node.h`).

The `module_name` needs to match the filename of the final binary (minus the
.node suffix).

The source code needs to be built into `addon.node`, the binary Addon. To
do this we create a file called `binding.gyp` which describes the configuration
to build your module in a JSON-like format. This file gets compiled by
[node-gyp](https://github.com/TooTallNate/node-gyp).

    {
      "targets": [
        {
          "target_name": "addon",
          "sources": [ "hello.cc" ]
        }
      ]
    }

The next step is to generate the appropriate project build files for the
current platform. Use `node-gyp configure` for that.

Now you will have either a `Makefile` (on Unix platforms) or a `vcxproj` file
(on Windows) in the `build/` directory. Next invoke the `node-gyp build`
command.

Now you have your compiled `.node` bindings file! The compiled bindings end up
in `build/Release/`.

You can now use the binary addon in an Node.js project `hello.js` by pointing
`require` to the recently built `hello.node` module:

    // hello.js
    var addon = require('./build/Release/addon');

    console.log(addon.hello()); // 'world'

Please see patterns below for further information or
<https://github.com/arturadib/node-qt> for an example in production.


## Addon patterns

Below are some addon patterns to help you get started. Consult the online
[v8 reference](http://izs.me/v8-docs/main.html) for help with the various v8
calls, and v8's [Embedder's Guide](http://code.google.com/apis/v8/embed.html)
for an explanation of several concepts used such as handles, scopes,
function templates, etc.

In order to use these examples you need to compile them using `node-gyp`.
Create the following `binding.gyp` file:

    {
      "targets": [
        {
          "target_name": "addon",
          "sources": [ "addon.cc" ]
        }
      ]
    }

In cases where there is more than one `.cc` file, simply add the file name to
the `sources` array, e.g.:

    "sources": ["addon.cc", "myexample.cc"]

Now that you have your `binding.gyp` ready, you can configure and build the
addon:

    $ node-gyp configure build


### Function arguments

The following pattern illustrates how to read arguments from JavaScript
function calls and return a result. This is the main and only needed source
`addon.cc`:

    // addon.cc
    #include <node.h>

    namespace demo {

    using v8::Exception;
    using v8::FunctionCallbackInfo;
    using v8::Isolate;
    using v8::Local;
    using v8::Number;
    using v8::Object;
    using v8::String;
    using v8::Value;

    void Add(const FunctionCallbackInfo<Value>& args) {
      Isolate* isolate = args.GetIsolate();

      if (args.Length() < 2) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Wrong number of arguments")));
        return;
      }

      if (!args[0]->IsNumber() || !args[1]->IsNumber()) {
        isolate->ThrowException(Exception::TypeError(
            String::NewFromUtf8(isolate, "Wrong arguments")));
        return;
      }

      double value = args[0]->NumberValue() + args[1]->NumberValue();
      Local<Number> num = Number::New(isolate, value);

      args.GetReturnValue().Set(num);
    }

    void Init(Local<Object> exports) {
      NODE_SET_METHOD(exports, "add", Add);
    }

    NODE_MODULE(addon, Init)

    }  // namespace demo

You can test it with the following JavaScript snippet:

    // test.js
    var addon = require('./build/Release/addon');

    console.log( 'This should be eight:', addon.add(3,5) );


### Callbacks

You can pass JavaScript functions to a C++ function and execute them from
there. Here's `addon.cc`:

    // addon.cc
    #include <node.h>

    namespace demo {

    using v8::Function;
    using v8::FunctionCallbackInfo;
    using v8::Isolate;
    using v8::Local;
    using v8::Null;
    using v8::Object;
    using v8::String;
    using v8::Value;

    void RunCallback(const FunctionCallbackInfo<Value>& args) {
      Isolate* isolate = args.GetIsolate();
      Local<Function> cb = Local<Function>::Cast(args[0]);
      const unsigned argc = 1;
      Local<Value> argv[argc] = { String::NewFromUtf8(isolate, "hello world") };
      cb->Call(Null(isolate), argc, argv);
    }

    void Init(Local<Object> exports, Local<Object> module) {
      NODE_SET_METHOD(module, "exports", RunCallback);
    }

    NODE_MODULE(addon, Init)

    }  // namespace demo

Note that this example uses a two-argument form of `Init()` that receives
the full `module` object as the second argument. This allows the addon
to completely overwrite `exports` with a single function instead of
adding the function as a property of `exports`.

To test it run the following JavaScript snippet:

    // test.js
    var addon = require('./build/Release/addon');

    addon(function(msg){
      console.log(msg); // 'hello world'
    });


### Object factory

You can create and return new objects from within a C++ function with this
`addon.cc` pattern, which returns an object with property `msg` that echoes
the string passed to `createObject()`:

    // addon.cc
    #include <node.h>

    namespace demo {

    using v8::FunctionCallbackInfo;
    using v8::Isolate;
    using v8::Local;
    using v8::Object;
    using v8::String;
    using v8::Value;

    void CreateObject(const FunctionCallbackInfo<Value>& args) {
      Isolate* isolate = args.GetIsolate();

      Local<Object> obj = Object::New(isolate);
      obj->Set(String::NewFromUtf8(isolate, "msg"), args[0]->ToString());

      args.GetReturnValue().Set(obj);
    }

    void Init(Local<Object> exports, Local<Object> module) {
      NODE_SET_METHOD(module, "exports", CreateObject);
    }

    NODE_MODULE(addon, Init)

    }  // namespace demo

To test it in JavaScript:

    // test.js
    var addon = require('./build/Release/addon');

    var obj1 = addon('hello');
    var obj2 = addon('world');
    console.log(obj1.msg+' '+obj2.msg); // 'hello world'


### Function factory

This pattern illustrates how to create and return a JavaScript function that
wraps a C++ function:

    // addon.cc
    #include <node.h>

    namespace demo {

    using v8::Function;
    using v8::FunctionCallbackInfo;
    using v8::FunctionTemplate;
    using v8::Isolate;
    using v8::Local;
    using v8::Object;
    using v8::String;
    using v8::Value;

    void MyFunction(const FunctionCallbackInfo<Value>& args) {
      Isolate* isolate = args.GetIsolate();
      args.GetReturnValue().Set(String::NewFromUtf8(isolate, "hello world"));
    }

    void CreateFunction(const FunctionCallbackInfo<Value>& args) {
      Isolate* isolate = args.GetIsolate();

      Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, MyFunction);
      Local<Function> fn = tpl->GetFunction();

      // omit this to make it anonymous
      fn->SetName(String::NewFromUtf8(isolate, "theFunction"));

      args.GetReturnValue().Set(fn);
    }

    void Init(Local<Object> exports, Local<Object> module) {
      NODE_SET_METHOD(module, "exports", CreateFunction);
    }

    NODE_MODULE(addon, Init)

    }  // namespace demo

To test:

    // test.js
    var addon = require('./build/Release/addon');

    var fn = addon();
    console.log(fn()); // 'hello world'


### Wrapping C++ objects

Here we will create a wrapper for a C++ object/class `MyObject` that can be
instantiated in JavaScript through the `new` operator. First prepare the main
module `addon.cc`:

    // addon.cc
    #include <node.h>
    #include "myobject.h"

    namespace demo {

    using v8::Local;
    using v8::Object;

    void InitAll(Local<Object> exports) {
      MyObject::Init(exports);
    }

    NODE_MODULE(addon, InitAll)

    }  // namespace demo

Then in `myobject.h` make your wrapper inherit from `node::ObjectWrap`:

    // myobject.h
    #ifndef MYOBJECT_H
    #define MYOBJECT_H

    #include <node.h>
    #include <node_object_wrap.h>

    namespace demo {

    class MyObject : public node::ObjectWrap {
     public:
      static void Init(v8::Local<v8::Object> exports);

     private:
      explicit MyObject(double value = 0);
      ~MyObject();

      static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
      static void PlusOne(const v8::FunctionCallbackInfo<v8::Value>& args);
      static v8::Persistent<v8::Function> constructor;
      double value_;
    };

    }  // namespace demo

    #endif

And in `myobject.cc` implement the various methods that you want to expose.
Here we expose the method `plusOne` by adding it to the constructor's
prototype:

    // myobject.cc
    #include "myobject.h"

    namespace demo {

    using v8::Function;
    using v8::FunctionCallbackInfo;
    using v8::FunctionTemplate;
    using v8::Isolate;
    using v8::Local;
    using v8::Number;
    using v8::Object;
    using v8::Persistent;
    using v8::String;
    using v8::Value;

    Persistent<Function> MyObject::constructor;

    MyObject::MyObject(double value) : value_(value) {
    }

    MyObject::~MyObject() {
    }

    void MyObject::Init(Local<Object> exports) {
      Isolate* isolate = exports->GetIsolate();

      // Prepare constructor template
      Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
      tpl->SetClassName(String::NewFromUtf8(isolate, "MyObject"));
      tpl->InstanceTemplate()->SetInternalFieldCount(1);

      // Prototype
      NODE_SET_PROTOTYPE_METHOD(tpl, "plusOne", PlusOne);

      constructor.Reset(isolate, tpl->GetFunction());
      exports->Set(String::NewFromUtf8(isolate, "MyObject"),
                   tpl->GetFunction());
    }

    void MyObject::New(const FunctionCallbackInfo<Value>& args) {
      Isolate* isolate = args.GetIsolate();

      if (args.IsConstructCall()) {
        // Invoked as constructor: `new MyObject(...)`
        double value = args[0]->IsUndefined() ? 0 : args[0]->NumberValue();
        MyObject* obj = new MyObject(value);
        obj->Wrap(args.This());
        args.GetReturnValue().Set(args.This());
      } else {
        // Invoked as plain function `MyObject(...)`, turn into construct call.
        const int argc = 1;
        Local<Value> argv[argc] = { args[0] };
        Local<Function> cons = Local<Function>::New(isolate, constructor);
        args.GetReturnValue().Set(cons->NewInstance(argc, argv));
      }
    }

    void MyObject::PlusOne(const FunctionCallbackInfo<Value>& args) {
      Isolate* isolate = args.GetIsolate();

      MyObject* obj = ObjectWrap::Unwrap<MyObject>(args.Holder());
      obj->value_ += 1;

      args.GetReturnValue().Set(Number::New(isolate, obj->value_));
    }

    }  // namespace demo

Test it with:

    // test.js
    var addon = require('./build/Release/addon');

    var obj = new addon.MyObject(10);
    console.log( obj.plusOne() ); // 11
    console.log( obj.plusOne() ); // 12
    console.log( obj.plusOne() ); // 13

### Factory of wrapped objects

This is useful when you want to be able to create native objects without
explicitly instantiating them with the `new` operator in JavaScript, e.g.

    var obj = addon.createObject();
    // instead of:
    // var obj = new addon.Object();

Let's register our `createObject` method in `addon.cc`:

    // addon.cc
    #include <node.h>
    #include "myobject.h"

    namespace demo {

    using v8::FunctionCallbackInfo;
    using v8::Isolate;
    using v8::Local;
    using v8::Object;
    using v8::String;
    using v8::Value;

    void CreateObject(const FunctionCallbackInfo<Value>& args) {
      MyObject::NewInstance(args);
    }

    void InitAll(Local<Object> exports, Local<Object> module) {
      MyObject::Init(exports->GetIsolate());

      NODE_SET_METHOD(module, "exports", CreateObject);
    }

    NODE_MODULE(addon, InitAll)

    }  // namespace demo

In `myobject.h` we now introduce the static method `NewInstance` that takes
care of instantiating the object (i.e. it does the job of `new` in JavaScript):

    // myobject.h
    #ifndef MYOBJECT_H
    #define MYOBJECT_H

    #include <node.h>
    #include <node_object_wrap.h>

    namespace demo {

    class MyObject : public node::ObjectWrap {
     public:
      static void Init(v8::Isolate* isolate);
      static void NewInstance(const v8::FunctionCallbackInfo<v8::Value>& args);

     private:
      explicit MyObject(double value = 0);
      ~MyObject();

      static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
      static void PlusOne(const v8::FunctionCallbackInfo<v8::Value>& args);
      static v8::Persistent<v8::Function> constructor;
      double value_;
    };

    }  // namespace demo

    #endif

The implementation is similar to the above in `myobject.cc`:

    // myobject.cc
    #include <node.h>
    #include "myobject.h"

    namespace demo {

    using v8::Function;
    using v8::FunctionCallbackInfo;
    using v8::FunctionTemplate;
    using v8::Isolate;
    using v8::Local;
    using v8::Number;
    using v8::Object;
    using v8::Persistent;
    using v8::String;
    using v8::Value;

    Persistent<Function> MyObject::constructor;

    MyObject::MyObject(double value) : value_(value) {
    }

    MyObject::~MyObject() {
    }

    void MyObject::Init(Isolate* isolate) {
      // Prepare constructor template
      Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
      tpl->SetClassName(String::NewFromUtf8(isolate, "MyObject"));
      tpl->InstanceTemplate()->SetInternalFieldCount(1);

      // Prototype
      NODE_SET_PROTOTYPE_METHOD(tpl, "plusOne", PlusOne);

      constructor.Reset(isolate, tpl->GetFunction());
    }

    void MyObject::New(const FunctionCallbackInfo<Value>& args) {
      Isolate* isolate = args.GetIsolate();

      if (args.IsConstructCall()) {
        // Invoked as constructor: `new MyObject(...)`
        double value = args[0]->IsUndefined() ? 0 : args[0]->NumberValue();
        MyObject* obj = new MyObject(value);
        obj->Wrap(args.This());
        args.GetReturnValue().Set(args.This());
      } else {
        // Invoked as plain function `MyObject(...)`, turn into construct call.
        const int argc = 1;
        Local<Value> argv[argc] = { args[0] };
        Local<Function> cons = Local<Function>::New(isolate, constructor);
        args.GetReturnValue().Set(cons->NewInstance(argc, argv));
      }
    }

    void MyObject::NewInstance(const FunctionCallbackInfo<Value>& args) {
      Isolate* isolate = args.GetIsolate();

      const unsigned argc = 1;
      Local<Value> argv[argc] = { args[0] };
      Local<Function> cons = Local<Function>::New(isolate, constructor);
      Local<Object> instance = cons->NewInstance(argc, argv);

      args.GetReturnValue().Set(instance);
    }

    void MyObject::PlusOne(const FunctionCallbackInfo<Value>& args) {
      Isolate* isolate = args.GetIsolate();

      MyObject* obj = ObjectWrap::Unwrap<MyObject>(args.Holder());
      obj->value_ += 1;

      args.GetReturnValue().Set(Number::New(isolate, obj->value_));
    }

    }  // namespace demo

Test it with:

    // test.js
    var createObject = require('./build/Release/addon');

    var obj = createObject(10);
    console.log( obj.plusOne() ); // 11
    console.log( obj.plusOne() ); // 12
    console.log( obj.plusOne() ); // 13

    var obj2 = createObject(20);
    console.log( obj2.plusOne() ); // 21
    console.log( obj2.plusOne() ); // 22
    console.log( obj2.plusOne() ); // 23


### Passing wrapped objects around

In addition to wrapping and returning C++ objects, you can pass them around
by unwrapping them with Node.js's `node::ObjectWrap::Unwrap` helper function.
In the following `addon.cc` we introduce a function `add()` that can take on two
`MyObject` objects:

    // addon.cc
    #include <node.h>
    #include <node_object_wrap.h>
    #include "myobject.h"

    namespace demo {

    using v8::FunctionCallbackInfo;
    using v8::Isolate;
    using v8::Local;
    using v8::Number;
    using v8::Object;
    using v8::String;
    using v8::Value;

    void CreateObject(const FunctionCallbackInfo<Value>& args) {
      MyObject::NewInstance(args);
    }

    void Add(const FunctionCallbackInfo<Value>& args) {
      Isolate* isolate = args.GetIsolate();

      MyObject* obj1 = node::ObjectWrap::Unwrap<MyObject>(
          args[0]->ToObject());
      MyObject* obj2 = node::ObjectWrap::Unwrap<MyObject>(
          args[1]->ToObject());

      double sum = obj1->value() + obj2->value();
      args.GetReturnValue().Set(Number::New(isolate, sum));
    }

    void InitAll(Local<Object> exports) {
      MyObject::Init(exports->GetIsolate());

      NODE_SET_METHOD(exports, "createObject", CreateObject);
      NODE_SET_METHOD(exports, "add", Add);
    }

    NODE_MODULE(addon, InitAll)

    }  // namespace demo

To make things interesting we introduce a public method in `myobject.h` so we
can probe private values after unwrapping the object:

    // myobject.h
    #ifndef MYOBJECT_H
    #define MYOBJECT_H

    #include <node.h>
    #include <node_object_wrap.h>

    namespace demo {

    class MyObject : public node::ObjectWrap {
     public:
      static void Init(v8::Isolate* isolate);
      static void NewInstance(const v8::FunctionCallbackInfo<v8::Value>& args);
      inline double value() const { return value_; }

     private:
      explicit MyObject(double value = 0);
      ~MyObject();

      static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
      static v8::Persistent<v8::Function> constructor;
      double value_;
    };

    }  // namespace demo

    #endif

The implementation of `myobject.cc` is similar as before:

    // myobject.cc
    #include <node.h>
    #include "myobject.h"

    namespace demo {

    using v8::Function;
    using v8::FunctionCallbackInfo;
    using v8::FunctionTemplate;
    using v8::Isolate;
    using v8::Local;
    using v8::Object;
    using v8::Persistent;
    using v8::String;
    using v8::Value;

    Persistent<Function> MyObject::constructor;

    MyObject::MyObject(double value) : value_(value) {
    }

    MyObject::~MyObject() {
    }

    void MyObject::Init(Isolate* isolate) {
      // Prepare constructor template
      Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
      tpl->SetClassName(String::NewFromUtf8(isolate, "MyObject"));
      tpl->InstanceTemplate()->SetInternalFieldCount(1);

      constructor.Reset(isolate, tpl->GetFunction());
    }

    void MyObject::New(const FunctionCallbackInfo<Value>& args) {
      Isolate* isolate = args.GetIsolate();

      if (args.IsConstructCall()) {
        // Invoked as constructor: `new MyObject(...)`
        double value = args[0]->IsUndefined() ? 0 : args[0]->NumberValue();
        MyObject* obj = new MyObject(value);
        obj->Wrap(args.This());
        args.GetReturnValue().Set(args.This());
      } else {
        // Invoked as plain function `MyObject(...)`, turn into construct call.
        const int argc = 1;
        Local<Value> argv[argc] = { args[0] };
        Local<Function> cons = Local<Function>::New(isolate, constructor);
        args.GetReturnValue().Set(cons->NewInstance(argc, argv));
      }
    }

    void MyObject::NewInstance(const FunctionCallbackInfo<Value>& args) {
      Isolate* isolate = args.GetIsolate();

      const unsigned argc = 1;
      Local<Value> argv[argc] = { args[0] };
      Local<Function> cons = Local<Function>::New(isolate, constructor);
      Local<Object> instance = cons->NewInstance(argc, argv);

      args.GetReturnValue().Set(instance);
    }

    }  // namespace demo

Test it with:

    // test.js
    var addon = require('./build/Release/addon');

    var obj1 = addon.createObject(10);
    var obj2 = addon.createObject(20);
    var result = addon.add(obj1, obj2);

    console.log(result); // 30

### AtExit hooks
#### void AtExit(callback, args)

* `callback`: `void (*)(void*)` - A pointer to the function to call at exit.
* `args`: `void*` - A pointer to pass to the callback at exit.

Registers exit hooks that run after the event loop has ended, but before the VM
is killed.

Callbacks are run in last-in, first-out order. AtExit takes two parameters:
a pointer to a callback function to run at exit, and a pointer to untyped
context data to be passed to that callback.

The file `addon.cc` implements AtExit below:

    // addon.cc
    #undef NDEBUG
    #include <assert.h>
    #include <stdlib.h>
    #include <node.h>

    namespace demo {

    using node::AtExit;
    using v8::HandleScope;
    using v8::Isolate;
    using v8::Local;
    using v8::Object;

    static char cookie[] = "yum yum";
    static int at_exit_cb1_called = 0;
    static int at_exit_cb2_called = 0;

    static void at_exit_cb1(void* arg) {
      Isolate* isolate = static_cast<Isolate*>(arg);
      HandleScope scope(isolate);
      Local<Object> obj = Object::New(isolate);
      assert(!obj.IsEmpty()); // assert VM is still alive
      assert(obj->IsObject());
      at_exit_cb1_called++;
    }

    static void at_exit_cb2(void* arg) {
      assert(arg == static_cast<void*>(cookie));
      at_exit_cb2_called++;
    }

    static void sanity_check(void*) {
      assert(at_exit_cb1_called == 1);
      assert(at_exit_cb2_called == 2);
    }

    void init(Local<Object> exports) {
      AtExit(sanity_check);
      AtExit(at_exit_cb2, cookie);
      AtExit(at_exit_cb2, cookie);
      AtExit(at_exit_cb1, exports->GetIsolate());
    }

    NODE_MODULE(addon, init);

    }  // namespace demo

Test in JavaScript by running:

    // test.js
    var addon = require('./build/Release/addon');
