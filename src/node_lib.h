#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <initializer_list>
#include "v8.h"
#include "node.h"

namespace node { namespace lib {

    namespace internal { // internals, made for experienced users

        v8::Isolate* isolate();

        Environment* environment();
    }

    bool EventLoopIsRunning();

    /*********************************************************
     * Function types
     *********************************************************/

    void _RegisterModuleCallback(v8::Local<v8::Object> exports,
              v8::Local<v8::Value> module,
              v8::Local<v8::Context> context,
              void* priv);


    /*********************************************************
     * Start Node.js engine
     *********************************************************/

    /*
    Starts the Node.js engine without a concrete script file to execute.
    *Important*: This requires the C++ developer to call `ProcessEvents()` periodically OR call `RunMainLoop()` to start the uv event loop.
    */
    NODE_EXTERN void Initialize(const std::string& program_name = "node_lib_executable", const std::vector<std::string>& node_args = {});


    /*
    Stops the existing Node.js engine. Eventloop should not be running at this point.
    *Important*: Once this was called, Initialize() will have to be called again for Node.js' library functions to be available again.
    */
    NODE_EXTERN int Deinitialize();

    /*
    Executes a given JavaScript file and returns once the execution has finished.
    *Important*: Node.js has to have been initialized by calling Initialize().
    */
    NODE_EXTERN v8::MaybeLocal<v8::Value> Run(const std::string & path);

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
    NODE_EXTERN v8::MaybeLocal<v8::Value> Evaluate(const std::string & java_script_code);

    /*
    Returns the JavaScript root object for the running application
    */
    NODE_EXTERN v8::MaybeLocal<v8::Object> GetRootObject();

    /*
    Registers a C++ module in the *running* Node.js engine.
    */
    NODE_EXTERN void RegisterModule(const std::string & name, const addon_context_register_func & callback, void *priv = nullptr, const std::string & target = "");

    /*
    Registers a C++ module in the *running* Node.js engine exporting the given set of functions.
    */
    NODE_EXTERN void RegisterModule(const std::string & name, const std::map<std::string, v8::FunctionCallback> & module_functions, const std::string & target = "");


    /*********************************************************
     * Convenience operations
     *********************************************************/

    /*
    Adds a new JavaScript module to the *running* Node.js engine.
    */
    NODE_EXTERN v8::MaybeLocal<v8::Object> IncludeModule(const std::string & module_name);

    /*
    Returns the local value (specified by its name) of the module (defined in the `exports`-object).
    */
    NODE_EXTERN v8::MaybeLocal<v8::Value> GetValue(v8::Local<v8::Object> object, const std::string & value_name);

    /*
    Calls a function (specified by its name) on a given object passing the given arguments.
    *Important*: Throws an exception if the receiver does not define the specified function.
    */
    NODE_EXTERN v8::MaybeLocal<v8::Value> Call(v8::Local<v8::Object> object, const std::string & function_name, const std::vector<v8::Local<v8::Value>> & args = {});

    /*
    Calls a function (specified by its name) on a given object passing the given arguments.
    *Important*: Throws an exception if the receiver does not define the specified function.
    */
    NODE_EXTERN v8::MaybeLocal<v8::Value> Call(v8::Local<v8::Object> object, const std::string & function_name, std::initializer_list<v8::Local<v8::Value>> args);

    /*
    Calls a given function on a given receiver passing the given arguments.
    *Important*: The amount of arguments can be changed at runtime (for JS var arg functions).
    */
    NODE_EXTERN v8::MaybeLocal<v8::Value> Call(v8::Local<v8::Object> receiver, v8::Local<v8::Function> function, const std::vector<v8::Local<v8::Value>> & args = {});

    /*
    Calls a given function on a given receiver passing the given arguments.
    *Important*: The amount of arguments must be known at compile time.
    */
    NODE_EXTERN v8::MaybeLocal<v8::Value> Call(v8::Local<v8::Object> receiver, v8::Local<v8::Function> function, std::initializer_list<v8::Local<v8::Value>> args);
}}
