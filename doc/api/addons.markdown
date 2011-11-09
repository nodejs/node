## Addons

Addons are dynamically linked shared objects. They can provide glue to C and
C++ libraries. The API (at the moment) is rather complex, involving
knowledge of several libraries:

 - V8 JavaScript, a C++ library. Used for interfacing with JavaScript:
   creating objects, calling functions, etc.  Documented mostly in the
   `v8.h` header file (`deps/v8/include/v8.h` in the Node source tree).

 - [libuv](https://github.com/joyent/libuv), C event loop library. Anytime one
   needs to wait for a file descriptor to become readable, wait for a timer, or
   wait for a signal to received one will need to interface with libuv. That is,
   if you perform any I/O, libuv will need to be used.

 - Internal Node libraries. Most importantly is the `node::ObjectWrap`
   class which you will likely want to derive from.

 - Others. Look in `deps/` for what else is available.

Node statically compiles all its dependencies into the executable. When
compiling your module, you don't need to worry about linking to any of these
libraries.

To get started let's make a small Addon which is the C++ equivalent of
the following Javascript code:

    exports.hello = function() { return 'world'; };

To get started we create a file `hello.cc`:

    #include <node.h>
    #include <v8.h>

    using namespace v8;

    Handle<Value> Method(const Arguments& args) {
      HandleScope scope;
      return scope.Close(String::New("world"));
    }

    void init(Handle<Object> target) {
      NODE_SET_METHOD(target, "hello", Method);
    }
    NODE_MODULE(hello, init)

Note that all Node addons must export an initialization function:

    void Initialize (Handle<Object> target);
    NODE_MODULE(module_name, Initialize)

There is no semi-colon after `NODE_MODULE` as it's not a function (see `node.h`).

The source code needs to be built into `hello.node`, the binary Addon. To
do this we create a file called `wscript` which is python code and looks
like this:

    srcdir = '.'
    blddir = 'build'
    VERSION = '0.0.1'

    def set_options(opt):
      opt.tool_options('compiler_cxx')

    def configure(conf):
      conf.check_tool('compiler_cxx')
      conf.check_tool('node_addon')

    def build(bld):
      obj = bld.new_task_gen('cxx', 'shlib', 'node_addon')
      obj.target = 'hello'
      obj.source = 'hello.cc'

Running `node-waf configure build` will create a file
`build/default/hello.node` which is our Addon.

`node-waf` is just [WAF](http://code.google.com/p/waf), the python-based build system. `node-waf` is
provided for the ease of users.

You can now use the binary addon in a Node project `hello.js` by pointing `require` to
the recently built module:

    var addon = require('./build/Release/hello');

    console.log(addon.hello()); // 'world'

For the moment, that is all the documentation on addons. Please see
<https://github.com/ry/node_postgres> for a real example.
