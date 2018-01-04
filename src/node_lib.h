#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <initializer_list>
#include "v8.h"

namespace node::lib {
    /*********************************************************
     * Function types
     *********************************************************/

    using std::function<void()> = RunUserLoop;

    using std::function<void(v8::Local<v8::Object> exports,
                               v8::Local<v8::Value> module,
                               void * private_data)> = RegisterModule;

    using std::function<void(v8::FunctionCallbackInfo<v8::Value> & info)> = ModuleFunction;


    /*********************************************************
     * Start Node.js engine
     *********************************************************/

    /*
    Starts the Node.js engine without a concrete script file to execute.
    *Important*: This requires the C++ developer to call `process_events()` periodically.
    */
    bool Initialize();

    /*
    Starts the Node.js engine with a given JavaScript file. Additionally, the Node.js engine will be kept alive
    calling the `callback` periodically.
    *Important*: This method will not return until the Node.js is stopped (e.g. by calling `terminate`).
    */
    bool StartMainLoop(const std::string & path, const RunUserLoop & callback);


    /*********************************************************
     * Handle JavaScript events
     *********************************************************/

    /*
    Processes the pending event queue of a *running* Node.js engine once.
    */
    bool ProcessEvents();

    /*
    Starts the execution of the Node.js main loop, which processes any events in JavaScript.
    Additionally, the given callback will be executed once per main loop run.
    *Important*: Call `initialize()` before using this method.
    */
    bool Run(const RunUserLoop & callback);


    /*********************************************************
     * Stop Node.js engine
     *********************************************************/

    /*
    Stops the *running* Node.js engine.
    */
    bool Terminate();


    /*********************************************************
     * Basic operations
     *********************************************************/

    /*
    Executes a given piece of JavaScript code, using the *running* Node.js engine.
    */
    bool Evaluate(const std::string & java_script_code);

    /*
    Returns the JavaScript root object for the running application
    */
    v8::MaybeLocal<v8::Object> GetRootObject();

    /*
    Registers a C++ module in the *running* Node.js engine.
    */
    bool RegisterModule(const std::string & name, const RegisterModule & callback);

    /*
    Registers a C++ module in the *running* Node.js engine exporting the given set of functions.
    */
    bool RegisterModule(const std::string & name,
                        const std::map<std::string, ModuleFunction> & module_functions);


    /*********************************************************
     * Convenience operations
     *********************************************************/

    /*
    Adds a new JavaScript module to the *running* Node.js engine.
    */
    v8::MaybeLocal<v8::Object> IncludeModule(const std::string & modul_name);

    /*
    Returns the local value (specified by its name) of the module (defined in the `exports`-object).
    */
    v8::MaybeLocal<v8::Value> GetValue(v8::MaybeLocal<v8::Object> object, const std::string & value_name);

    /*
    Calls a function (specified by its name) on a given object passing the given arguments.
    *Important*: Throws an exception if the receiver does not define the specified function.
    */
    v8::Local<v8::Value> Call(v8::MaybeLocal<v8::Object> object, const std::string & function_name, const std::vector<v8::MaybeLocal<v8::Value>> & args = {});

    /*
    Calls a function (specified by its name) on a given object passing the given arguments.
    *Important*: Throws an exception if the receiver does not define the specified function.
    */
    v8::Local<v8::Value> Call(v8::MaybeLocal<v8::Object> object, const std::string & function_name, std::initializer_list<v8::MaybeLocal<v8::Value>> args);

    /*
    Calls a given function on a given receiver passing the given arguments.
    *Important*: The amount of arguments can be changed at runtime (for JS var arg functions).
    */
    v8::Local<v8::Value> Call(v8::MaybeLocal<v8::Object> receiver, v8::MaybeLocal<v8::Function> function, const std::vector<v8::MaybeLocal<v8::Value>> & args = {});

    /*
    Calls a given function on a given receiver passing the given arguments.
    *Important*: The amount of arguments must be known at compile time.
    */
    v8::Local<v8::Value> Call(v8::MaybeLocal<v8::Object> receiver, v8::MaybeLocal<v8::Function> function, std::initializer_list<v8::MaybeLocal<v8::Value>> args);
}
