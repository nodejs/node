# C++ Embedder API

<!--introduced_in=REPLACEME-->

Node.js provides a number of C++ APIs that can be used to execute JavaScript
in a Node.js environment from other C++ software.

The documentation for these APIs can be found in [src/node.h][] in the Node.js
source tree. In addition to the APIs exposed by Node.js, some required concepts
are provided by the V8 embedder API.

Because using Node.js as an embedded library is different from writing code
that is executed by Node.js, breaking changes do not follow typical Node.js
[deprecation policy][] and may occur on each semver-major release without prior
warning.

## Example embedding application

The following sections will provide an overview over how to use these APIs
to create an application from scratch that will perform the equivalent of
`node -e <code>`, i.e. that will take a piece of JavaScript and run it in
a Node.js-specific environment.

The full code can be found [in the Node.js source tree][embedtest.cc].

### Setting up per-process state

Node.js requires some per-process state management in order to run:

* Arguments parsing for Node.js [CLI options][],
* V8 per-process requirements, such as a `v8::Platform` instance.

The following example shows how these can be set up. Some class names are from
the `node` and `v8` C++ namespaces, respectively.

```c++
int main(int argc, char** argv) {
  std::vector<std::string> args(argv, argv + argc);
  std::vector<std::string> exec_args;
  std::vector<std::string> errors;
  // Parse Node.js CLI options, and print any errors that have occurred while
  // trying to parse them.
  int exit_code = node::InitializeNodeWithArgs(&args, &exec_args, &errors);
  for (const std::string& error : errors)
    fprintf(stderr, "%s: %s\n", args[0].c_str(), error.c_str());
  if (exit_code != 0) {
    return exit_code;
  }

  // Create a v8::Platform instance. `MultiIsolatePlatform::Create()` is a way
  // to create a v8::Platform instance that Node.js can use when creating
  // Worker threads. When no `MultiIsolatePlatform` instance is present,
  // Worker threads are disabled.
  std::unique_ptr<MultiIsolatePlatform> platform =
      MultiIsolatePlatform::Create(4);
  V8::InitializePlatform(platform.get());
  V8::Initialize();

  // See below for the contents of this function.
  int ret = RunNodeInstance(platform.get(), args, exec_args);

  V8::Dispose();
  V8::ShutdownPlatform();
  return ret;
}
```

### Per-instance state

Node.js has a concept of a “Node.js instance”, that is commonly being referred
to as `node::Environment`. Each `node::Environment` is associated with:

* Exactly one `v8::Isolate`, i.e. one JS Engine instance,
* Exactly one `uv_loop_t`, i.e. one event loop, and
* A number of `v8::Context`s, but exactly one main `v8::Context`.
* One `node::IsolateData` instance that contains information that could be
  shared by multiple `node::Environment`s that use the same `v8::Isolate`.
  Currently, no testing if performed for this scenario.

In order to set up a `v8::Isolate`, an `v8::ArrayBuffer::Allocator` needs
to be provided. One possible choice is the default Node.js allocator, which
can be created through `node::ArrayBufferAllocator::Create()`. Using the Node.js
allocator allows minor performance optimizations when addons use the Node.js
C++ `Buffer` API, and is required in order to track `ArrayBuffer` memory in
[`process.memoryUsage()`][].

Additionally, each `v8::Isolate` that is used for a Node.js instance needs to
be registered and unregistered with the `MultiIsolatePlatform` instance, if one
is being used, in order for the platform to know which event loop to use
for tasks scheduled by the `v8::Isolate`.

The `node::NewIsolate()` helper function creates a `v8::Isolate`,
sets it up with some Node.js-specific hooks (e.g. the Node.js error handler),
and registers it with the platform automatically.

```c++
int RunNodeInstance(MultiIsolatePlatform* platform,
                    const std::vector<std::string>& args,
                    const std::vector<std::string>& exec_args) {
  int exit_code = 0;
  // Set up a libuv event loop.
  uv_loop_t loop;
  int ret = uv_loop_init(&loop);
  if (ret != 0) {
    fprintf(stderr, "%s: Failed to initialize loop: %s\n",
            args[0].c_str(),
            uv_err_name(ret));
    return 1;
  }

  std::shared_ptr<ArrayBufferAllocator> allocator =
      ArrayBufferAllocator::Create();

  Isolate* isolate = NewIsolate(allocator, &loop, platform);
  if (isolate == nullptr) {
    fprintf(stderr, "%s: Failed to initialize V8 Isolate\n", args[0].c_str());
    return 1;
  }

  {
    Locker locker(isolate);
    Isolate::Scope isolate_scope(isolate);

    // Create a node::IsolateData instance that will later be released using
    // node::FreeIsolateData().
    std::unique_ptr<IsolateData, decltype(&node::FreeIsolateData)> isolate_data(
        node::CreateIsolateData(isolate, &loop, platform, allocator.get()),
        node::FreeIsolateData);

    // Set up a new v8::Context.
    HandleScope handle_scope(isolate);
    Local<Context> context = node::NewContext(isolate);
    if (context.IsEmpty()) {
      fprintf(stderr, "%s: Failed to initialize V8 Context\n", args[0].c_str());
      return 1;
    }

    // The v8::Context needs to be entered when node::CreateEnvironment() and
    // node::LoadEnvironment() are being called.
    Context::Scope context_scope(context);

    // Create a node::Environment instance that will later be released using
    // node::FreeEnvironment().
    std::unique_ptr<Environment, decltype(&node::FreeEnvironment)> env(
        node::CreateEnvironment(isolate_data.get(), context, args, exec_args),
        node::FreeEnvironment);

    // Set up the Node.js instance for execution, and run code inside of it.
    // There is also a variant that takes a callback and provides it with
    // the `require` and `process` objects, so that it can manually compile
    // and run scripts as needed.
    // The `require` function inside this script does *not* access the file
    // system, and can only load built-in Node.js modules.
    // `module.createRequire()` is being used to create one that is able to
    // load files from the disk, and uses the standard CommonJS file loader
    // instead of the internal-only `require` function.
    MaybeLocal<Value> loadenv_ret = node::LoadEnvironment(
        env.get(),
        "const publicRequire ="
        "  require('module').createRequire(process.cwd() + '/');"
        "globalThis.require = publicRequire;"
        "require('vm').runInThisContext(process.argv[1]);");

    if (loadenv_ret.IsEmpty())  // There has been a JS exception.
      return 1;

    {
      // SealHandleScope protects against handle leaks from callbacks.
      SealHandleScope seal(isolate);
      bool more;
      do {
        uv_run(&loop, UV_RUN_DEFAULT);

        // V8 tasks on background threads may end up scheduling new tasks in the
        // foreground, which in turn can keep the event loop going. For example,
        // WebAssembly.compile() may do so.
        platform->DrainTasks(isolate);

        // If there are new tasks, continue.
        more = uv_loop_alive(&loop);
        if (more) continue;

        // node::EmitBeforeExit() is used to emit the 'beforeExit' event on
        // the `process` object.
        node::EmitBeforeExit(env.get());

        // 'beforeExit' can also schedule new work that keeps the event loop
        // running.
        more = uv_loop_alive(&loop);
      } while (more == true);
    }

    // node::EmitExit() returns the current exit code.
    exit_code = node::EmitExit(env.get());

    // node::Stop() can be used to explicitly stop the event loop and keep
    // further JavaScript from running. It can be called from any thread,
    // and will act like worker.terminate() if called from another thread.
    node::Stop(env.get());
  }

  // Unregister the Isolate with the platform and add a listener that is called
  // when the Platform is done cleaning up any state it had associated with
  // the Isolate.
  bool platform_finished = false;
  platform->AddIsolateFinishedCallback(isolate, [](void* data) {
    *static_cast<bool*>(data) = true;
  }, &platform_finished);
  platform->UnregisterIsolate(isolate);
  isolate->Dispose();

  // Wait until the platform has cleaned up all relevant resources.
  while (!platform_finished)
    uv_run(&loop, UV_RUN_ONCE);
  int err = uv_loop_close(&loop);
  assert(err == 0);

  return exit_code;
}
```

[`process.memoryUsage()`]: process.html#process_process_memoryusage
[CLI options]: cli.html
[deprecation policy]: deprecations.html
[embedtest.cc]: https://github.com/nodejs/node/blob/master/test/embedding/embedtest.cc
[src/node.h]: https://github.com/nodejs/node/blob/master/src/node.h
