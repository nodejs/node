// Copyright 2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_V8_DEBUG_H_
#define V8_V8_DEBUG_H_

#include "v8.h"  // NOLINT(build/include)

/**
 * ATTENTION: The debugger API exposed by this file is deprecated and will be
 *            removed by the end of 2017. Please use the V8 inspector declared
 *            in include/v8-inspector.h instead.
 */
namespace v8 {

// Debug events which can occur in the V8 JavaScript engine.
enum DebugEvent {
  Break = 1,
  Exception = 2,
  AfterCompile = 3,
  CompileError = 4,
  AsyncTaskEvent = 5,
};

class V8_EXPORT Debug {
 public:
  /**
   * A client object passed to the v8 debugger whose ownership will be taken by
   * it. v8 is always responsible for deleting the object.
   */
  class ClientData {
   public:
    virtual ~ClientData() {}
  };


  /**
   * A message object passed to the debug message handler.
   */
  class Message {
   public:
    /**
     * Check type of message.
     */
    virtual bool IsEvent() const = 0;
    virtual bool IsResponse() const = 0;
    virtual DebugEvent GetEvent() const = 0;

    /**
     * Indicate whether this is a response to a continue command which will
     * start the VM running after this is processed.
     */
    virtual bool WillStartRunning() const = 0;

    /**
     * Access to execution state and event data. Don't store these cross
     * callbacks as their content becomes invalid. These objects are from the
     * debugger event that started the debug message loop.
     */
    virtual Local<Object> GetExecutionState() const = 0;
    virtual Local<Object> GetEventData() const = 0;

    /**
     * Get the debugger protocol JSON.
     */
    virtual Local<String> GetJSON() const = 0;

    /**
     * Get the context active when the debug event happened. Note this is not
     * the current active context as the JavaScript part of the debugger is
     * running in its own context which is entered at this point.
     */
    virtual Local<Context> GetEventContext() const = 0;

    /**
     * Client data passed with the corresponding request if any. This is the
     * client_data data value passed into Debug::SendCommand along with the
     * request that led to the message or NULL if the message is an event. The
     * debugger takes ownership of the data and will delete it even if there is
     * no message handler.
     */
    virtual ClientData* GetClientData() const = 0;

    virtual Isolate* GetIsolate() const = 0;

    virtual ~Message() {}
  };

  /**
   * An event details object passed to the debug event listener.
   */
  class EventDetails {
   public:
    /**
     * Event type.
     */
    virtual DebugEvent GetEvent() const = 0;

    /**
     * Access to execution state and event data of the debug event. Don't store
     * these cross callbacks as their content becomes invalid.
     */
    virtual Local<Object> GetExecutionState() const = 0;
    virtual Local<Object> GetEventData() const = 0;

    /**
     * Get the context active when the debug event happened. Note this is not
     * the current active context as the JavaScript part of the debugger is
     * running in its own context which is entered at this point.
     */
    virtual Local<Context> GetEventContext() const = 0;

    /**
     * Client data passed with the corresponding callback when it was
     * registered.
     */
    virtual Local<Value> GetCallbackData() const = 0;

    /**
     * This is now a dummy that returns nullptr.
     */
    virtual ClientData* GetClientData() const = 0;

    virtual Isolate* GetIsolate() const = 0;

    virtual ~EventDetails() {}
  };

  /**
   * Debug event callback function.
   *
   * \param event_details object providing information about the debug event
   *
   * A EventCallback does not take possession of the event data,
   * and must not rely on the data persisting after the handler returns.
   */
  typedef void (*EventCallback)(const EventDetails& event_details);

  /**
   * This is now a no-op.
   */
  typedef void (*MessageHandler)(const Message& message);

  V8_DEPRECATED("No longer supported", static bool SetDebugEventListener(
                                           Isolate* isolate, EventCallback that,
                                           Local<Value> data = Local<Value>()));

  // Schedule a debugger break to happen when JavaScript code is run
  // in the given isolate.
  V8_DEPRECATED("No longer supported",
                static void DebugBreak(Isolate* isolate));

  // Remove scheduled debugger break in given isolate if it has not
  // happened yet.
  V8_DEPRECATED("No longer supported",
                static void CancelDebugBreak(Isolate* isolate));

  // Check if a debugger break is scheduled in the given isolate.
  V8_DEPRECATED("No longer supported",
                static bool CheckDebugBreak(Isolate* isolate));

  // This is now a no-op.
  V8_DEPRECATED("No longer supported",
                static void SetMessageHandler(Isolate* isolate,
                                              MessageHandler handler));

  // This is now a no-op.
  V8_DEPRECATED("No longer supported",
                static void SendCommand(Isolate* isolate,
                                        const uint16_t* command, int length,
                                        ClientData* client_data = NULL));

  /**
   * Run a JavaScript function in the debugger.
   * \param fun the function to call
   * \param data passed as second argument to the function
   * With this call the debugger is entered and the function specified is called
   * with the execution state as the first argument. This makes it possible to
   * get access to information otherwise not available during normal JavaScript
   * execution e.g. details on stack frames. Receiver of the function call will
   * be the debugger context global object, however this is a subject to change.
   * The following example shows a JavaScript function which when passed to
   * v8::Debug::Call will return the current line of JavaScript execution.
   *
   * \code
   *   function frame_source_line(exec_state) {
   *     return exec_state.frame(0).sourceLine();
   *   }
   * \endcode
   */
  V8_DEPRECATED("No longer supported",
                static MaybeLocal<Value> Call(
                    Local<Context> context, v8::Local<v8::Function> fun,
                    Local<Value> data = Local<Value>()));

  // This is now a no-op.
  V8_DEPRECATED("No longer supported",
                static void ProcessDebugMessages(Isolate* isolate));

  /**
   * Debugger is running in its own context which is entered while debugger
   * messages are being dispatched. This is an explicit getter for this
   * debugger context. Note that the content of the debugger context is subject
   * to change. The Context exists only when the debugger is active, i.e. at
   * least one DebugEventListener or MessageHandler is set.
   */
  V8_DEPRECATED("Use v8-inspector",
                static Local<Context> GetDebugContext(Isolate* isolate));

  /**
   * While in the debug context, this method returns the top-most non-debug
   * context, if it exists.
   */
  V8_DEPRECATED(
      "No longer supported",
      static MaybeLocal<Context> GetDebuggedContext(Isolate* isolate));

  /**
   * Enable/disable LiveEdit functionality for the given Isolate
   * (default Isolate if not provided). V8 will abort if LiveEdit is
   * unexpectedly used. LiveEdit is enabled by default.
   */
  V8_DEPRECATED("No longer supported",
                static void SetLiveEditEnabled(Isolate* isolate, bool enable));

  /**
   * Returns array of internal properties specific to the value type. Result has
   * the following format: [<name>, <value>,...,<name>, <value>]. Result array
   * will be allocated in the current context.
   */
  V8_DEPRECATED("No longer supported",
                static MaybeLocal<Array> GetInternalProperties(
                    Isolate* isolate, Local<Value> value));

  /**
   * Defines if the ES2015 tail call elimination feature is enabled or not.
   * The change of this flag triggers deoptimization of all functions that
   * contain calls at tail position.
   */
  V8_DEPRECATED("No longer supported",
                static bool IsTailCallEliminationEnabled(Isolate* isolate));
  V8_DEPRECATED("No longer supported",
                static void SetTailCallEliminationEnabled(Isolate* isolate,
                                                          bool enabled));
};


}  // namespace v8


#undef EXPORT


#endif  // V8_V8_DEBUG_H_
