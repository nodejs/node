#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <initializer_list>
#include "v8.h"

namespace node { namespace lib {
    void _StartEnv(int argc,
                   const char* const* argv);

    int _StopEnv();


    /*********************************************************
     * Function types
     *********************************************************/

    using RunUserLoop = std::function<void()>;

    using CppModule = std::function<void(v8::Local<v8::Object> exports,
                               v8::Local<v8::Value> module,
                               void * private_data)>;

    using ModuleFunction = std::function<void(v8::FunctionCallbackInfo<v8::Value> & info)>;


    /*********************************************************
     * Start Node.js engine
     *********************************************************/

    /*
    Starts the Node.js engine without a concrete script file to execute.
    *Important*: This requires the C++ developer to call `ProcessEvents()` periodically OR call `Run()` to start the uv event loop.
    */
    NODE_EXTERN void Initialize(const std::string& program_name);

    /*
    Starts the Node.js engine with a given JavaScript file. Additionally, the Node.js engine will be kept alive
    calling the `callback` periodically.
    *Important*: This method will not return until the Node.js is stopped (e.g. by calling `terminate`).
    */
    //bool StartMainLoop(const std::string & path, const RunUserLoop & callback);

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
    *Important*: Call `initialize()` before using this method.
    */
    NODE_EXTERN void RunEventLoop(const RunUserLoop & callback);


    /*********************************************************
     * Stop Node.js engine
     *********************************************************/

    /*
    Sends termination event into event queue and runs event queue, until all events have been handled.
    */
    NODE_EXTERN void Terminate();


    /*
    Stops the *running* Node.js engine. Clears all events and puts Node.js into idle.
    */
    NODE_EXTERN void RequestTerminate();

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
    NODE_EXTERN bool RegisterModule(const std::string & name, const CppModule & callback);

    /*
    Registers a C++ module in the *running* Node.js engine exporting the given set of functions.
    */
    NODE_EXTERN bool RegisterModule(const std::string & name,
                        const std::map<std::string, ModuleFunction> & module_functions);


    /*********************************************************
     * Convenience operations
     *********************************************************/

    /*
    Adds a new JavaScript module to the *running* Node.js engine.
    */
    NODE_EXTERN v8::Local<v8::Object> IncludeModule(const std::string & modul_name);

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
}}
