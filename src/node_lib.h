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

        /**
         * @brief Internal function to register a native C++ module.
         * 
         * Internally used function to register a new C++ module for Node.js.
         * @param exports The exports object, which is exposed to JavaScript.
         * @param module The v8 module.
         * @param context The current v8 context.
         * @param priv Private data for the module.
         */
        void _RegisterModuleCallback(v8::Local<v8::Object> exports,
              v8::Local<v8::Value> module,
              v8::Local<v8::Context> context,
              void* priv);
    }

    /**
     * @brief Indicates, whether the Node.js event loop is executed by `RunEventLoop`.
     * @return True, if the Node.js event loop is executed by `RunEventLoop`. False otherwise. 
     */
    bool EventLoopIsRunning() { return _event_loop_running; }

    /*********************************************************
     * Start Node.js engine
     *********************************************************/

    /**
     * @brief Starts the Node.js engine without executing a concrete script.
     * 
     * Starts the Node.js engine by executing bootstrap code.
     * This is required in order to load scripts (e.g. `Run`) or evaluate JavaScript code (e.g. `Evaluate`).
     * Additionally, Node.js will not process any pending events caused by the JavaScript execution as long as
     * `ProcessEvents` or `RunMainLoop` is not called.
     * @param program_name The name for the Node.js application.
     */
    NODE_EXTERN void Initialize(const std::string& program_name = "node_lib_executable", const std::vector<std::string>& node_args = {});

    /**
     * @brief Stops the Node.js engine and destroys all current state.
     * 
     * Stops the Node.js engine.
     * This is done in two steps:
     * 1. Issues the Node.js event loop to no longer accept any incoming events.
     * 2. Waits for the event loop to be empty and then executes clean up code.
     */
    NODE_EXTERN int Deinitialize();

    /**
     * @brief Executes the content of a given JavaScript file.
     * 
     * Loads and executes the content of the given file.
     * This method returns after the script was evaluated once.
     * This means, that any pending events will not be processes as long as 
     * `ProcessEvents` or `RunEventLoop` is not called.
     * @param path The path to the JavaScript file.
     * @return The return value of the given JavaScript file.
     */
    NODE_EXTERN v8::MaybeLocal<v8::Value> Run(const std::string & path);

    /*********************************************************
     * Handle JavaScript events
     *********************************************************/

    /**
     * @brief Executes the Node.js event loop once.
     * 
     * Processes all currently pending events in the Node.js event loop.
     * This method returns immediately if there are no pending events.
     * @return True, if more events need to be processed. False otherwise.
     */
    NODE_EXTERN bool ProcessEvents();

    /**
     * @brief Starts the execution of the Node.js event loop. Calling the given callback once per loop tick.
     * 
     * Executes the Node.js event loop as long as events keep coming.
     * Once per loop execution, after events were processed, the given callback is executed.
     * The event loop can be paused by calling `StopEventLoop`.
     * @param callback The callback, which should be executed periodically while the calling thread is blocked.
     */
    NODE_EXTERN void RunEventLoop(const std::function<void()> & callback);


    /*********************************************************
     * Stop Node.js engine
     *********************************************************/

    /**
     * @brief Issues the Node.js event loop to stop.
     * 
     * Issues the Node.js event loop to stop.
     * The event loop will finish its current execution.
     * This means, that the loop is not stopped when this method returns.
     * The execution can be resumed by using `RunEventLoop` again.
     */
    NODE_EXTERN void StopEventLoop();

    /*********************************************************
     * Basic operations
     *********************************************************/

    /**
     * @brief Evaluates the given JavaScript code.
     * 
     * Parses and runs the given JavaScipt code.
     * @param java_script_code The code to evaluate.
     * @return The return value of the evaluated code.
     */
    NODE_EXTERN v8::MaybeLocal<v8::Value> Evaluate(const std::string & java_script_code);

    /**
     * @brief Returns the JavaScript root object.
     * 
     * Returns the global root object for the current JavaScript context.
     * @return The global root object.
     */
    NODE_EXTERN v8::MaybeLocal<v8::Object> GetRootObject();

    /**
     * @brief Registers a native C++ module.
     * 
     * Adds a native module to the Node.js engine.
     * The module is initialized within the given callback. Additionally, private data can be included in the module
     * using the priv pointer.
     * The module can be used in JavaScript by calling `let cpp_module = process.binding('module_name')`.
     * @param name The name for the module.
     * @param callback The method, which initializes the module (e.g. by adding methods to the module).
     * @param priv Any private data, which should be included within the module.
     */
    NODE_EXTERN void RegisterModule(const std::string & name, const addon_context_register_func & callback, void *priv = nullptr, const std::string & target = "");

    /**
     * @brief Registers a native C++ module.
     * 
     * Adds a native module to the Node.js engine.
     * Additionally, this method adds the given methods to the module.
     * The module can be used in JavaScript by calling `let cpp_module = process.binding('module_name')`.
     * @param name The name for the module.
     * @param module_functions A list of functions and their names for the module.
     */
    NODE_EXTERN void RegisterModule(const std::string & name, const std::map<std::string, v8::FunctionCallback> & module_functions, const std::string & target = "");


    /*********************************************************
     * Convenience operations
     *********************************************************/

    /**
     * @brief Adds a NPM module to the current JavaScript context.
     * 
     * Adds a given NPM module to the JavaScript context.
     * This is achieved by calling `require('module_name')`.
     * *Important* Make sure the NPM module is installed before using this method.
     * @param module_name The name of the NPM module.
     * @return The export object of the NPM module.
     */
    NODE_EXTERN v8::MaybeLocal<v8::Object> IncludeModule(const std::string & module_name);

    /**
     * @brief Returns a member of the given object.
     *
     * Returns a member of the given object, specified by the members name.
     * @param object The container for the requested value.
     * @param value_name The name of the requested value.
     * @return The requested value.
     */
    NODE_EXTERN v8::MaybeLocal<v8::Value> GetValue(v8::Local<v8::Object> object, const std::string & value_name);

    /**
     * @brief Calls a method on a given object.
     * 
     * Calls a method on a given object.
     * The function is retrieved by using the functions name.
     * Additionally, a list of parameters is passed to the called function.
     * @param object The container of the called function.
     * @param function_name The name of the function to call.
     * @param args The parameters to pass to the called function.
     * @return The return value of the called function.
     */
    NODE_EXTERN v8::MaybeLocal<v8::Value> Call(v8::Local<v8::Object> object, const std::string & function_name, const std::vector<v8::Local<v8::Value>> & args = {});

    /**
     * @brief Calls a method on a given object.
     * 
     * Calls a method on a given object.
     * The function is retrieved by using the functions name.
     * Additionally, a list of parameters is passed to the called function.
     * @param object The container of the called function.
     * @param function_name The name of the function to call.
     * @param args The parameters to pass to the called function. The amount of arguments must be known at compile time.
     * @return The return value of the called function.
     */
    NODE_EXTERN v8::MaybeLocal<v8::Value> Call(v8::Local<v8::Object> object, const std::string & function_name, std::initializer_list<v8::Local<v8::Value>> args);

    /**
     * @brief Calls a given method on a given object.
     * 
     * Calls a given method on a given object.
     * Additionally, a list of parameters is passed to the called function.
     * @param object The receiver of the given function.
     * @param function The function to be called.
     * @param args The parameters to pass to the called function.
     * @return The return value of the called function.
     */
    NODE_EXTERN v8::MaybeLocal<v8::Value> Call(v8::Local<v8::Object> receiver, v8::Local<v8::Function> function, const std::vector<v8::Local<v8::Value>> & args = {});

    /**
     * @brief Calls a given method on a given object.
     * 
     * Calls a given method on a given object.
     * Additionally, a list of parameters is passed to the called function.
     * @param object The receiver of the given function.
     * @param function The function to be called.
     * @param args The parameters to pass to the called function. The amount of arguments must be known at compile time.
     * @return The return value of the called function.
     */
    NODE_EXTERN v8::MaybeLocal<v8::Value> Call(v8::Local<v8::Object> receiver, v8::Local<v8::Function> function, std::initializer_list<v8::Local<v8::Value>> args);
}}
