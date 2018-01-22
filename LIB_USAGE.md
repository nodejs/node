## How to use Node.js as a C++ library
### How to build
TODO

### Handling the Node.js event loop
There are two different ways of handling the Node.js event loop.
#### C++ keeps control over thread
By calling `node::lib::ProcessEvents()`, the Node.js event loop will be run once, handling the next pending event. The return value of the call specifies whether there are more events in the queue.

#### C++ gives up control of the thread to Node.js
By calling `node::lib::RunMainLoop(callback)`, the C++ host program gives up the control of the thread and allows the Node.js event loop to run indefinitely until `node::lib::StopEventLoop()` is called. The `callback` parameter in the `RunMainLoop` function is called once per event loop. This allows the C++ programmer to react on changes in the Node.js state and e.g. terminate Node.js preemptively. 

### Examples

#### (1) Evaluating in-line JS code
This example evaluates a single line of JS code in the global Node context. The result of `console.log` is piped to the stdout.

```C++
node::lib::Initialize("example01");
node::lib::Evaluate("var helloMessage = 'Hello from Node.js!';");
node::lib::Evaluate("console.log(helloMessage);");
```

#### (2) Running a JS file
This example evaluates a JS file and lets Node handle all pending events until the event loop is empty.

```C++
node::lib::Initialize("example02");
node::lib::Run("cli.js");
while (node::lib::ProcessEvents()) { }
``` 


#### (3) Including an NPM Module
This example uses the [fs](https://nodejs.org/api/fs.html) module to check whether a specific file exists.
```C++
node::lib::Initialize("example03");
auto fs = node::lib::IncludeModule("fs");
v8::Isolate *isolate = v8::Isolate::GetCurrent();


// Check if file cli-test.js exists in the current working directory.
auto result = node::lib::Call(fs, "existsSync", {v8::String::NewFromUtf8(isolate, "cli.js")});

auto file_exists = v8::Local<v8::Boolean>::Cast(result)->BooleanValue();
std::cout << (file_exists ? "cli.js exists in cwd" : "cli.js does NOT exist in cwd") << std::endl;

```

Copy-pasted header file:
```C++    
/*
Starts the Node.js engine without a concrete script file to execute.
*Important*: This requires the C++ developer to call `ProcessEvents()` periodically OR call `RunMainLoop()` to start the uv event loop.
*/
NODE_EXTERN void Initialize(const std::string& program_name = "node_lib_executable");


/*
Stops the existing Node.js engine. Eventloop should not be running at this point.
*Important*: Once this was called, Initialize() will have to be called again for Node.js' library functions to be available again.
*/
NODE_EXTERN int Deinitialize();

/*
Executes a given JavaScript file and returns once the execution has finished.
*Important*: Node.js has to have been initialized by calling Initialize().
*/
NODE_EXTERN v8::Local<v8::Value> Run(const std::string & path);

/*********************************************************
 * Handle JavaScript events
 *********************************************************/

/*
Processes the pending event queue of a *running* Node.js engine once.
*/
NODE_EXTERN bool ProcessEvents();

/*
Starts the execution of the Node.js event loop, which processes any events in JavaScript.
Additionally, the given callback will be executed once per main loop run.
*Important*: Call `Initialize()` before using this method.
*/
NODE_EXTERN void RunEventLoop(const std::function<void()> & callback);


/*********************************************************
 * Stop Node.js engine
 *********************************************************/

/*
Stops the Node.js event loop after its current execution. Execution can be resumed by calling RunEventLoop() again.
*/
NODE_EXTERN void StopEventLoop();

/*********************************************************
 * Basic operations
 *********************************************************/

/*
Executes a given piece of JavaScript code, using the *running* Node.js engine.
*/
NODE_EXTERN v8::Local<v8::Value> Evaluate(const std::string & java_script_code);

/*
Returns the JavaScript root object for the running application
*/
NODE_EXTERN v8::Local<v8::Object> GetRootObject();

/*
Registers a C++ module in the *running* Node.js engine.
*/
NODE_EXTERN void RegisterModule(const std::string & name, const addon_context_register_func & callback, void *priv = nullptr);

/*
Registers a C++ module in the *running* Node.js engine exporting the given set of functions.
*/
NODE_EXTERN void RegisterModule(const std::string & name,
                    const std::map<std::string, v8::FunctionCallback> & module_functions);


/*********************************************************
 * Convenience operations
 *********************************************************/

/*
Adds a new JavaScript module to the *running* Node.js engine.
*/
NODE_EXTERN v8::Local<v8::Object> IncludeModule(const std::string & module_name);

/*
Returns the local value (specified by its name) of the module (defined in the `exports`-object).
*/
NODE_EXTERN v8::MaybeLocal<v8::Value> GetValue(v8::MaybeLocal<v8::Object> object, const std::string & value_name);

/*
Calls a function (specified by its name) on a given object passing the given arguments.
*Important*: Throws an exception if the receiver does not define the specified function.
*/
NODE_EXTERN v8::Local<v8::Value> Call(v8::Local<v8::Object> object, const std::string & function_name, const std::vector<v8::Local<v8::Value>> & args = {});

/*
Calls a function (specified by its name) on a given object passing the given arguments.
*Important*: Throws an exception if the receiver does not define the specified function.
*/
NODE_EXTERN v8::Local<v8::Value> Call(v8::Local<v8::Object> object, const std::string & function_name, std::initializer_list<v8::Local<v8::Value>> args);

/*
Calls a given function on a given receiver passing the given arguments.
*Important*: The amount of arguments can be changed at runtime (for JS var arg functions).
*/
NODE_EXTERN v8::Local<v8::Value> Call(v8::MaybeLocal<v8::Object> receiver, v8::MaybeLocal<v8::Function> function, const std::vector<v8::MaybeLocal<v8::Value>> & args = {});

/*
Calls a given function on a given receiver passing the given arguments.
*Important*: The amount of arguments must be known at compile time.
*/
NODE_EXTERN v8::Local<v8::Value> Call(v8::MaybeLocal<v8::Object> receiver, v8::MaybeLocal<v8::Function> function, std::initializer_list<v8::MaybeLocal<v8::Value>> args);

```