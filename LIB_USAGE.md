## How to use Node.js as a C++ library
### Handling the Node.js event loop
There are two different ways of handling the Node.js event loop.
#### C++ keeps control over thread
By calling `node::lib::ProcessEvents()`, the Node.js event loop will be run once, handling the next pending event. The return value of the call specifies whether there are more events in the queue.

#### C++ gives up control of the thread to Node.js
By calling `node::lib::RunMainLoop(callback)`, the C++ host program gives up the control of the thread and allows the Node.js event loop to run indefinitely until `node::lib::StopEventLoop()` is called. The `callback` parameter in the `RunMainLoop` function is called once per event loop. This allows the C++ programmer to react on changes in the Node.js state and e.g. terminate Node.js preemptively. 

### Examples

In the following, simple examples demonstrate the usage of Node.js as a library. For more complex examples, including handling of the event loop, see the [node-embed](https://github.com/hpicgs/node-embed) repository.

#### (1) Evaluating in-line JavaScript code
This example evaluates multiple lines of JavaScript code in the global Node context. The result of `console.log` is piped to the stdout.

```C++
node::lib::Initialize("example01");
node::lib::Evaluate("var helloMessage = 'Hello from Node.js!';");
node::lib::Evaluate("console.log(helloMessage);");
```

#### (2) Running a JavaScript file
This example evaluates a JavaScript file and lets Node handle all pending events until the event loop is empty.

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

// Check if file cli.js exists in the current working directory.
auto result = node::lib::Call(fs, "existsSync", {v8::String::NewFromUtf8(isolate, "cli.js")});

auto file_exists = v8::Local<v8::Boolean>::Cast(result)->BooleanValue();
std::cout << (file_exists ? "cli.js exists in cwd" : "cli.js does NOT exist in cwd") << std::endl;

```
