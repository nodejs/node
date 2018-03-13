#ifndef SRC_NODE_LIB_H_
#define SRC_NODE_LIB_H_

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <initializer_list>
#include "v8.h"
#include "uv.h"
#include "node.h"

namespace node {

namespace internal {  // internals, provided for experienced users

/**
 * @brief Returns the `v8::Isolate` for Node.js.
 *
 * Returns a pointer to the currently used `v8::Isolate`, if the Node.js engine
 * is initialized already.
 * *Important* Use with caution, changing this object might break Node.js.
 * @return Pointer to the `v8::Isolate`.
 */
v8::Isolate* isolate();

/**
 * @brief Returns the `node::Environment` for Node.js.
 *
 * Returns a pointer to the currently used `node::Environment`, if the Node.js
 * engine is initialized already.
 * *Important* Use with caution, changing this object might break Node.js.
 * @return Pointer to the `node::Environment`.
 */
Environment* environment();

}  // namespace internal

/**
 * @brief Configures the uv loop behavior, which is used within the Node.js
 * event loop.
 *
 * Contains various behavior patterns for the uv loop, which is used within
 * the Node.js event loop.
 * *Important*: Contains the same values as `uv_run_mode`.
 */
enum class UvLoopBehavior : int {
  RUN_DEFAULT  = UV_RUN_DEFAULT,
  ///< Processes events as long as events are available.

  RUN_ONCE     = UV_RUN_ONCE,
  ///< Processes events once from the uv loop.
  ///< If there are currently no events, the loop will wait until at least
  ///< one event appeared.

  RUN_NOWAIT   = UV_RUN_NOWAIT,
  ///< Processes events once from the uv loop. If there are currently no events,
  ///< the loop will *not* wait und return immediately.
};

/**
 * @brief Indicates if the Node.js event loop is executed by `RunEventLoop`.
 * @return True, if the Node.js event loop is executed by `RunEventLoop`.
 * False otherwise.
 */
bool eventLoopIsRunning();

/*********************************************************
 * Start Node.js engine
 *********************************************************/

/**
 * @brief Starts the Node.js engine without executing a concrete script.
 *
 * Starts the Node.js engine by executing bootstrap code.
 * This is required in order to load scripts (e.g. `Run`) or evaluate
 * JavaScript code (e.g. `Evaluate`).
 * Additionally, Node.js will not process any pending events caused by the
 * JavaScript execution as long as `ProcessEvents` or `RunEventLoop` is
 * not called.
 * @param program_name The name for the Node.js application.
 * @param node_args List of arguments for the Node.js engine.
 * @param evaluate_stdin Controls whether stdin is evaluated.
 * @return Potential errors are indicated by a return value not equal to 0.
 */
NODE_EXTERN int Initialize(const std::string& program_name = "node_lib",
                           const std::vector<std::string>& node_args = {},
                           const bool evaluate_stdin = false);

/**
 * @brief Starts the Node.js engine.
 *
 * Starts the Node.js engine by executing bootstrap code.
 * This is required in order to load scripts (e.g. `Run`) or evaluate
 * JavaScript code (e.g. `Evaluate`).
 * Additionally, Node.js will not process any pending events caused by the
 * JavaScript execution as long as `ProcessEvents` or `RunEventLoop` is
 * not called.
 * @param argc The number of arguments.
 * @param argv List of arguments for the Node.js engine,
 * where the first argument needs to be the program name.
 * The number of arguments must correspond to argc.
 * @param evaluate_stdin Controls whether stdin is evaluated.
 * @return Potential errors are indicated by a return value not equal to 0.
 */
NODE_EXTERN int Initialize(int argc,
                           const char** argv,
                           const bool evaluate_stdin = false);


/**
 * @brief Stops the Node.js engine and destroys all current state.
 *
 * Stops the Node.js engine.
 * This is done in two steps:
 * 1. Issues the Node.js event loop to no longer accept any incoming events.
 * 2. Waits for the event loop to be empty and then executes clean up code.
 */
NODE_EXTERN int Deinitialize();

/*********************************************************
 * Handle JavaScript events
 *********************************************************/

/**
 * @brief Executes the Node.js event loop once.
 *
 * Processes all currently pending events in the Node.js event loop.
 * This method returns immediately if there are no pending events.
 * @param behavior The uv event loop behavior.
 * @return True, if more events need to be processed. False otherwise.
 */
NODE_EXTERN bool ProcessEvents(
      UvLoopBehavior behavior = UvLoopBehavior::RUN_NOWAIT);

/**
 * @brief Starts the execution of the Node.js event loop. Calling the given
 * callback once per loop tick.
 *
 * Executes the Node.js event loop as long as events keep coming.
 * Once per loop execution, after events were processed, the given callback
 * is executed. The event loop can be paused by calling `StopEventLoop`.
 * @param behavior The uv event loop behavior.
 * @param callback The callback, which should be executed periodically while
 * the calling thread is blocked.
 */
NODE_EXTERN void RunEventLoop(
      const std::function<void()> & callback,
      UvLoopBehavior behavior = UvLoopBehavior::RUN_NOWAIT);

/*********************************************************
 * Stop Node.js engine
 *********************************************************/

/**
 * @brief Issues the Node.js event loop to stop.
 *
 * Issues the Node.js event loop to stop.
 * The event loop will finish its current execution. This means, that the loop
 * is not guaranteed to have stopped when this method returns.
 * The execution can be resumed by using `RunEventLoop` again.
 */
NODE_EXTERN void StopEventLoop();

/*********************************************************
 * Basic operations
 *********************************************************/

/**
 * @brief Executes the content of a given JavaScript file.
 *
 * Loads and executes the content of the given file.
 * This method returns after the script was evaluated once.
 * This means, that any pending events will not be processed as long as
 * `ProcessEvents` or `RunEventLoop` is not called.
 * Note: This method is executed in the environment created by the
 * Initialize() function.
 * @param path The path to the JavaScript file.
 * @return The return value of the given JavaScript file.
 */
NODE_EXTERN v8::MaybeLocal<v8::Value> Run(const std::string& path);

/**
 * @brief Executes the content of a given JavaScript file.
 *
 * Loads and executes the content of the given file.
 * This method returns after the script was evaluated once.
 * This means, that any pending events will not be processed as long as
 * `ProcessEvents` or `RunEventLoop` is not called.
 * @param env The environment where this call should be executed.
 * @param path The path to the JavaScript file.
 * @return The return value of the given JavaScript file.
 */
NODE_EXTERN v8::MaybeLocal<v8::Value> Run(Environment* env,
                                          const std::string& path);

/**
 * @brief Evaluates the given JavaScript code.
 *
 * Parses and runs the given JavaScipt code.
 * Note: This method is executed in the environment created by the
 * Initialize() function.
 * @param java_script_code The code to evaluate.
 * @return The return value of the evaluated code.
 */
NODE_EXTERN v8::MaybeLocal<v8::Value> Evaluate(const std::string& js_code);

/**
 * @brief Evaluates the given JavaScript code.
 *
 * Parses and runs the given JavaScipt code.
 * @param env The environment where this call should be executed.
 * @param java_script_code The code to evaluate.
 * @return The return value of the evaluated code.
 */
NODE_EXTERN v8::MaybeLocal<v8::Value> Evaluate(Environment* env,
                                               const std::string& js_code);

/**
 * @brief Returns the JavaScript root object.
 *
 * Returns the global root object for the current JavaScript context.
 * Note: This method is executed in the environment created by the
 * Initialize() function.
 * @return The global root object.
 */
NODE_EXTERN v8::MaybeLocal<v8::Object> GetRootObject();
/**
 * @brief Returns the JavaScript root object.
 *
 * Returns the global root object for the current JavaScript context.
 * @param env The environment where this call should be executed.
 * @return The global root object.
 */
NODE_EXTERN v8::MaybeLocal<v8::Object> GetRootObject(Environment* env);

/**
 * @brief Registers a native C++ module.
 *
 * Adds a native module to the Node.js engine.
 * The module is initialized within the given callback. Additionally, private
 * data can be included in the module using the priv pointer.
 * The module can be used in JavaScript by calling
 * `let cpp_module = process.binding('module_name')`.
 * Note: This method is executed in the environment created by the
 * Initialize() function.
 * @param name The name for the module.
 * @param callback The method, which initializes the module (e.g. by adding
 * methods to the module).
 * @param priv Any private data, which should be included within the module.
 * @param target The name for the module within the JavaScript context. (e.g.
 * `const target = process.binding(module_name)`) If empty, the module
 * will *not* be registered within the global JavaScript context automatically.
 */
NODE_EXTERN void RegisterModule(const std::string& name,
                                const addon_context_register_func& callback,
                                void* priv = nullptr,
                                const std::string& target = "");
/**
 * @brief Registers a native C++ module.
 *
 * Adds a native module to the Node.js engine.
 * The module is initialized within the given callback. Additionally, private
 * data can be included in the module using the priv pointer.
 * The module can be used in JavaScript by calling
 * `let cpp_module = process.binding('module_name')`.
 * @param env The environment where this call should be executed.
 * @param name The name for the module.
 * @param callback The method, which initializes the module (e.g. by adding
 * methods to the module).
 * @param priv Any private data, which should be included within the module.
 * @param target The name for the module within the JavaScript context. (e.g.
 * `const target = process.binding(module_name)`) If empty, the module
 * will *not* be registered within the global JavaScript context automatically.
 */
NODE_EXTERN void RegisterModule(Environment* env,
                                const std::string& name,
                                const addon_context_register_func& callback,
                                void* priv = nullptr,
                                const std::string& target = "");

/**
 * @brief Registers a native C++ module.
 *
 * Adds a native module to the Node.js engine.
 * Additionally, this method adds the given methods to the module.
 * The module can be used in JavaScript by calling
 * `let cpp_module = process.binding('module_name')`.
 * Note: This method is executed in the environment created by the
 * Initialize() function.
 * @param name The name for the module.
 * @param module_functions A list of functions and their names for the module.
 * @param target The name for the module within the JavaScript context. (e.g.
 * `const target = process.binding(module_name)`) If empty, the module
 * will *not* be registered within the global JavaScript context automatically.
 */
NODE_EXTERN void RegisterModule(
    const std::string& name,
    const std::map<std::string, v8::FunctionCallback>& module_functions,
    const std::string& target = "");

/**
 * @brief Registers a native C++ module.
 *
 * Adds a native module to the Node.js engine.
 * Additionally, this method adds the given methods to the module.
 * The module can be used in JavaScript by calling
 * `let cpp_module = process.binding('module_name')`.
 * @param env The environment where this call should be executed.
 * @param name The name for the module.
 * @param module_functions A list of functions and their names for the module.
 * @param target The name for the module within the JavaScript context. (e.g.
 * `const target = process.binding(module_name)`) If empty, the module
 * will *not* be registered within the global JavaScript context automatically.
 */
NODE_EXTERN void RegisterModule(
    Environment* env,
    const std::string& name,
    const std::map<std::string, v8::FunctionCallback>& module_functions,
    const std::string& target = "");


/*********************************************************
 * Convenience operations
 *********************************************************/

/**
 * @brief Adds a NPM module to the current JavaScript context.
 *
 * Adds a given NPM module to the JavaScript context.
 * This is achieved by calling `require('module_name')`.
 * *Important* Make sure the NPM module is installed before using this method.
 * Note: This method is executed in the environment created by the
 * Initialize() function.
 * @param name The name of the NPM module.
 * When using just the modules name, the "node_modules" directory should be
 * located within the working directory. You can also load modules from
 * different locations by providing the full path to the module.
 * @return The export object of the NPM module.
 */
NODE_EXTERN v8::MaybeLocal<v8::Object> IncludeModule(const std::string& name);

/**
 * @brief Adds a NPM module to the current JavaScript context.
 *
 * Adds a given NPM module to the JavaScript context.
 * This is achieved by calling `require('module_name')`.
 * *Important* Make sure the NPM module is installed before using this method.
 * @param env The environment where this call should be executed.
 * @param name The name of the NPM module.
 * When using just the modules name, the "node_modules" directory should be
 * located within the working directory. You can also load modules from
 * different locations by providing the full path to the module.
 * @return The export object of the NPM module.
 */
NODE_EXTERN v8::MaybeLocal<v8::Object> IncludeModule(Environment* env,
                                                     const std::string& name);

/**
 * @brief Returns a member of the given object.
 *
 * Returns a member of the given object, specified by the members name.
 * Note: This method is executed in the environment created by the
 * Initialize() function.
 * @param object The container for the requested value.
 * @param value_name The name of the requested value.
 * @return The requested value.
 */
NODE_EXTERN v8::MaybeLocal<v8::Value> GetValue(v8::Local<v8::Object> object,
                                               const std::string& value_name);

/**
 * @brief Returns a member of the given object.
 *
 * Returns a member of the given object, specified by the members name.
 * @param env The environment where this call should be executed.
 * @param object The container for the requested value.
 * @param value_name The name of the requested value.
 * @return The requested value.
 */
NODE_EXTERN v8::MaybeLocal<v8::Value> GetValue(Environment* env,
                                               v8::Local<v8::Object> object,
                                               const std::string& value_name);

/**
 * @brief Calls a method on a given object.
 *
 * Calls a method on a given object.
 * The function is retrieved by using the functions name.
 * Additionally, a list of parameters is passed to the called function.
 * Note: This method is executed in the environment created by the
 * Initialize() function.
 * @param object The container of the called function.
 * @param function_name The name of the function to call.
 * @param args The parameters to pass to the called function.
 * @return The return value of the called function.
 */
NODE_EXTERN v8::MaybeLocal<v8::Value> Call(
    v8::Local<v8::Object> object,
    const std::string& function_name,
    const std::vector<v8::Local<v8::Value>>& args = {});

/**
 * @brief Calls a method on a given object.
 *
 * Calls a method on a given object.
 * The function is retrieved by using the functions name.
 * Additionally, a list of parameters is passed to the called function.
 * @param env The environment where this call should be executed.
 * @param object The container of the called function.
 * @param function_name The name of the function to call.
 * @param args The parameters to pass to the called function.
 * @return The return value of the called function.
 */
NODE_EXTERN v8::MaybeLocal<v8::Value> Call(
    Environment* env,
    v8::Local<v8::Object> object,
    const std::string& function_name,
    const std::vector<v8::Local<v8::Value>>& args = {});

/**
 * @brief Calls a given method on a given object.
 *
 * Calls a given method on a given object.
 * Additionally, a list of parameters is passed to the called function.
 * Note: This method is executed in the environment created by the
 * Initialize() function.
 * @param object The receiver of the given function.
 * @param function The function to be called.
 * @param args The parameters to pass to the called function.
 * @return The return value of the called function.
 */
NODE_EXTERN v8::MaybeLocal<v8::Value> Call(
    v8::Local<v8::Object> receiver,
    v8::Local<v8::Function> function,
    const std::vector<v8::Local<v8::Value>>& args = {});

/**
 * @brief Calls a given method on a given object.
 *
 * Calls a given method on a given object.
 * Additionally, a list of parameters is passed to the called function.
 * @param env The environment where this call should be executed.
 * @param object The receiver of the given function.
 * @param function The function to be called.
 * @param args The parameters to pass to the called function.
 * @return The return value of the called function.
 */
NODE_EXTERN v8::MaybeLocal<v8::Value> Call(
    Environment* env,
    v8::Local<v8::Object> receiver,
    v8::Local<v8::Function> function,
    const std::vector<v8::Local<v8::Value>>& args = {});

#endif  // SRC_NODE_LIB_H_
