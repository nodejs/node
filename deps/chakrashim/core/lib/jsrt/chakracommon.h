//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
/// \mainpage Chakra Hosting API Reference
///
/// Chakra is Microsoft's JavaScript engine. It is an integral part of Internet Explorer but can
/// also be hosted independently by other applications. This reference describes the APIs available
/// to applications to host Chakra.
///
/// This file contains the common API set shared among all Chakra releases.  For Windows-specific
/// releases, see chakrart.h.

/// \file
/// \brief The base Chakra hosting API.
///
/// This file contains a flat C API layer. This is the API exported by chakra.dll.

#ifdef _MSC_VER
#pragma once
#endif  // _MSC_VER

#ifndef _CHAKRACOMMON_H_
#define _CHAKRACOMMON_H_

#include <oaidl.h>



    /// <summary>
    ///     An error code returned from a Chakra hosting API.
    /// </summary>
    typedef _Return_type_success_(return == 0) enum _JsErrorCode : unsigned int
    {
        /// <summary>
        ///     Success error code.
        /// </summary>
        JsNoError = 0,

        /// <summary>
        ///     Category of errors that relates to incorrect usage of the API itself.
        /// </summary>
        JsErrorCategoryUsage = 0x10000,
        /// <summary>
        ///     An argument to a hosting API was invalid.
        /// </summary>
        JsErrorInvalidArgument,
        /// <summary>
        ///     An argument to a hosting API was null in a context where null is not allowed.
        /// </summary>
        JsErrorNullArgument,
        /// <summary>
        ///     The hosting API requires that a context be current, but there is no current context.
        /// </summary>
        JsErrorNoCurrentContext,
        /// <summary>
        ///     The engine is in an exception state and no APIs can be called until the exception is
        ///     cleared.
        /// </summary>
        JsErrorInExceptionState,
        /// <summary>
        ///     A hosting API is not yet implemented.
        /// </summary>
        JsErrorNotImplemented,
        /// <summary>
        ///     A hosting API was called on the wrong thread.
        /// </summary>
        JsErrorWrongThread,
        /// <summary>
        ///     A runtime that is still in use cannot be disposed.
        /// </summary>
        JsErrorRuntimeInUse,
        /// <summary>
        ///     A bad serialized script was used, or the serialized script was serialized by a
        ///     different version of the Chakra engine.
        /// </summary>
        JsErrorBadSerializedScript,
        /// <summary>
        ///     The runtime is in a disabled state.
        /// </summary>
        JsErrorInDisabledState,
        /// <summary>
        ///     Runtime does not support reliable script interruption.
        /// </summary>
        JsErrorCannotDisableExecution,
        /// <summary>
        ///     A heap enumeration is currently underway in the script context.
        /// </summary>
        JsErrorHeapEnumInProgress,
        /// <summary>
        ///     A hosting API that operates on object values was called with a non-object value.
        /// </summary>
        JsErrorArgumentNotObject,
        /// <summary>
        ///     A script context is in the middle of a profile callback.
        /// </summary>
        JsErrorInProfileCallback,
        /// <summary>
        ///     A thread service callback is currently underway.
        /// </summary>
        JsErrorInThreadServiceCallback,
        /// <summary>
        ///     Scripts cannot be serialized in debug contexts.
        /// </summary>
        JsErrorCannotSerializeDebugScript,
        /// <summary>
        ///     The context cannot be put into a debug state because it is already in a debug state.
        /// </summary>
        JsErrorAlreadyDebuggingContext,
        /// <summary>
        ///     The context cannot start profiling because it is already profiling.
        /// </summary>
        JsErrorAlreadyProfilingContext,
        /// <summary>
        ///     Idle notification given when the host did not enable idle processing.
        /// </summary>
        JsErrorIdleNotEnabled,
        /// <summary>
        ///     The context did not accept the enqueue callback.
        /// </summary>
        JsCannotSetProjectionEnqueueCallback,
        /// <summary>
        ///     Failed to start projection.
        /// </summary>
        JsErrorCannotStartProjection,
        /// <summary>
        ///     The operation is not supported in an object before collect callback.
        /// </summary>
        JsErrorInObjectBeforeCollectCallback,
        /// <summary>
        ///     Object cannot be unwrapped to IInspectable pointer.
        /// </summary>
        JsErrorObjectNotInspectable,
        /// <summary>
        ///     A hosting API that operates on symbol property ids but was called with a non-symbol property id.
        ///     The error code is returned by JsGetSymbolFromPropertyId if the function is called with non-symbol property id.
        /// </summary>
        JsErrorPropertyNotSymbol,
        /// <summary>
        ///     A hosting API that operates on string property ids but was called with a non-string property id.
        ///     The error code is returned by existing JsGetPropertyNamefromId if the function is called with non-string property id.
        /// </summary>
        JsErrorPropertyNotString,

        /// <summary>
        ///     Category of errors that relates to errors occurring within the engine itself.
        /// </summary>
        JsErrorCategoryEngine = 0x20000,
        /// <summary>
        ///     The Chakra engine has run out of memory.
        /// </summary>
        JsErrorOutOfMemory,

        /// <summary>
        ///     Category of errors that relates to errors in a script.
        /// </summary>
        JsErrorCategoryScript = 0x30000,
        /// <summary>
        ///     A JavaScript exception occurred while running a script.
        /// </summary>
        JsErrorScriptException,
        /// <summary>
        ///     JavaScript failed to compile.
        /// </summary>
        JsErrorScriptCompile,
        /// <summary>
        ///     A script was terminated due to a request to suspend a runtime.
        /// </summary>
        JsErrorScriptTerminated,
        /// <summary>
        ///     A script was terminated because it tried to use <c>eval</c> or <c>function</c> and eval
        ///     was disabled.
        /// </summary>
        JsErrorScriptEvalDisabled,

        /// <summary>
        ///     Category of errors that are fatal and signify failure of the engine.
        /// </summary>
        JsErrorCategoryFatal = 0x40000,
        /// <summary>
        ///     A fatal error in the engine has occurred.
        /// </summary>
        JsErrorFatal,
        /// <summary>
        ///     A hosting API was called with object created on different javascript runtime.
        /// </summary>
        JsErrorWrongRuntime,
    } JsErrorCode;

    /// <summary>
    ///     A handle to a Chakra runtime.
    /// </summary>
    /// <remarks>
    ///     <para>
    ///     Each Chakra runtime has its own independent execution engine, JIT compiler, and garbage
    ///     collected heap. As such, each runtime is completely isolated from other runtimes.
    ///     </para>
    ///     <para>
    ///     Runtimes can be used on any thread, but only one thread can call into a runtime at any
    ///     time.
    ///     </para>
    ///     <para>
    ///     NOTE: A <c>JsRuntimeHandle</c>, unlike other object references in the Chakra hosting API,
    ///     is not garbage collected since it contains the garbage collected heap itself. A runtime
    ///     will continue to exist until <c>JsDisposeRuntime</c> is called.
    ///     </para>
    /// </remarks>
    typedef void *JsRuntimeHandle;

    /// <summary>
    ///     An invalid runtime handle.
    /// </summary>
    const JsRuntimeHandle JS_INVALID_RUNTIME_HANDLE = NULL;

    /// <summary>
    ///     A reference to an object owned by the Chakra garbage collector.
    /// </summary>
    /// <remarks>
    ///     A Chakra runtime will automatically track <c>JsRef</c> references as long as they are
    ///     stored in local variables or in parameters (i.e. on the stack). Storing a <c>JsRef</c>
    ///     somewhere other than on the stack requires calling <c>JsAddRef</c> and <c>JsRelease</c> to
    ///     manage the lifetime of the object, otherwise the garbage collector may free the object
    ///     while it is still in use.
    /// </remarks>
    typedef void *JsRef;

    /// <summary>
    ///     An invalid reference.
    /// </summary>
    const JsRef JS_INVALID_REFERENCE = NULL;

    /// <summary>
    ///     A reference to a script context.
    /// </summary>
    /// <remarks>
    ///     <para>
    ///     Each script context contains its own global object, distinct from the global object in
    ///     other script contexts.
    ///     </para>
    ///     <para>
    ///     Many Chakra hosting APIs require an "active" script context, which can be set using
    ///     <c>JsSetCurrentContext</c>. Chakra hosting APIs that require a current context to be set
    ///     will note that explicitly in their documentation.
    ///     </para>
    /// </remarks>
    typedef JsRef JsContextRef;

    /// <summary>
    ///     A reference to a JavaScript value.
    /// </summary>
    /// <remarks>
    ///     A JavaScript value is one of the following types of values: undefined, null, Boolean,
    ///     string, number, or object.
    /// </remarks>
    typedef JsRef JsValueRef;

    /// <summary>
    ///     A cookie that identifies a script for debugging purposes.
    /// </summary>
    typedef DWORD_PTR JsSourceContext;

    /// <summary>
    ///     An empty source context.
    /// </summary>
    const JsSourceContext JS_SOURCE_CONTEXT_NONE = (JsSourceContext)-1;

    /// <summary>
    ///     A property identifier.
    /// </summary>
    /// <remarks>
    ///     Property identifiers are used to refer to properties of JavaScript objects instead of using
    ///     strings.
    /// </remarks>
    typedef JsRef JsPropertyIdRef;

    /// <summary>
    ///     Attributes of a runtime.
    /// </summary>
    typedef enum _JsRuntimeAttributes
    {
        /// <summary>
        ///     No special attributes.
        /// </summary>
        JsRuntimeAttributeNone = 0x00000000,
        /// <summary>
        ///     The runtime will not do any work (such as garbage collection) on background threads.
        /// </summary>
        JsRuntimeAttributeDisableBackgroundWork = 0x00000001,
        /// <summary>
        ///     The runtime should support reliable script interruption. This increases the number of
        ///     places where the runtime will check for a script interrupt request at the cost of a
        ///     small amount of runtime performance.
        /// </summary>
        JsRuntimeAttributeAllowScriptInterrupt = 0x00000002,
        /// <summary>
        ///     Host will call <c>JsIdle</c>, so enable idle processing. Otherwise, the runtime will
        ///     manage memory slightly more aggressively.
        /// </summary>
        JsRuntimeAttributeEnableIdleProcessing = 0x00000004,
        /// <summary>
        ///     Runtime will not generate native code.
        /// </summary>
        JsRuntimeAttributeDisableNativeCodeGeneration = 0x00000008,
        /// <summary>
        ///     Using <c>eval</c> or <c>function</c> constructor will throw an exception.
        /// </summary>
        JsRuntimeAttributeDisableEval = 0x00000010,
        /// <summary>
        ///     Runtime will enable all experimental features.
        /// </summary>
        JsRuntimeAttributeEnableExperimentalFeatures = 0x00000020,
        /// <summary>
        ///     Calling <c>JsSetException</c> will also dispatch the exception to the script debugger
        ///     (if any) giving the debugger a chance to break on the exception.
        /// </summary>
        JsRuntimeAttributeDispatchSetExceptionsToDebugger = 0x00000040
    } JsRuntimeAttributes;

    /// <summary>
    ///     The type of a typed JavaScript array.
    /// </summary>
    typedef enum _JsTypedArrayType
    {
        /// <summary>
        ///     An int8 array.
        /// </summary>
        JsArrayTypeInt8,
        /// <summary>
        ///     An uint8 array.
        /// </summary>
        JsArrayTypeUint8,
        /// <summary>
        ///     An uint8 clamped array.
        /// </summary>
        JsArrayTypeUint8Clamped,
        /// <summary>
        ///     An int16 array.
        /// </summary>
        JsArrayTypeInt16,
        /// <summary>
        ///     An uint16 array.
        /// </summary>
        JsArrayTypeUint16,
        /// <summary>
        ///     An int32 array.
        /// </summary>
        JsArrayTypeInt32,
        /// <summary>
        ///     An uint32 array.
        /// </summary>
        JsArrayTypeUint32,
        /// <summary>
        ///     A float32 array.
        /// </summary>
        JsArrayTypeFloat32,
        /// <summary>
        ///     A float64 array.
        /// </summary>
        JsArrayTypeFloat64
    } JsTypedArrayType;

    /// <summary>
    ///     Allocation callback event type.
    /// </summary>
    typedef enum _JsMemoryEventType
    {
        /// <summary>
        ///     Indicates a request for memory allocation.
        /// </summary>
        JsMemoryAllocate = 0,
        /// <summary>
        ///     Indicates a memory freeing event.
        /// </summary>
        JsMemoryFree = 1,
        /// <summary>
        ///     Indicates a failed allocation event.
        /// </summary>
        JsMemoryFailure = 2
    } JsMemoryEventType;

    /// <summary>
    ///     User implemented callback routine for memory allocation events
    /// </summary>
    /// <remarks>
    ///     Use <c>JsSetRuntimeMemoryAllocationCallback</c> to register this callback.
    /// </remarks>
    /// <param name="callbackState">
    ///     The state passed to <c>JsSetRuntimeMemoryAllocationCallback</c>.
    /// </param>
    /// <param name="allocationEvent">The type of type allocation event.</param>
    /// <param name="allocationSize">The size of the allocation.</param>
    /// <returns>
    ///     For the <c>JsMemoryAllocate</c> event, returning <c>true</c> allows the runtime to continue
    ///     with the allocation. Returning false indicates the allocation request is rejected. The
    ///     return value is ignored for other allocation events.
    /// </returns>
    typedef bool (CALLBACK * JsMemoryAllocationCallback)(_In_opt_ void *callbackState, _In_ JsMemoryEventType allocationEvent, _In_ size_t allocationSize);

    /// <summary>
    ///     A callback called before collection.
    /// </summary>
    /// <remarks>
    ///     Use <c>JsSetBeforeCollectCallback</c> to register this callback.
    /// </remarks>
    /// <param name="callbackState">The state passed to <c>JsSetBeforeCollectCallback</c>.</param>
    typedef void (CALLBACK *JsBeforeCollectCallback)(_In_opt_ void *callbackState);

    /// <summary>
    ///     A callback called before collecting an object.
    /// </summary>
    /// <remarks>
    ///     Use <c>JsSetObjectBeforeCollectCallback</c> to register this callback.
    /// </remarks>
    /// <param name="ref">The object to be collected.</param>
    /// <param name="callbackState">The state passed to <c>JsSetObjectBeforeCollectCallback</c>.</param>
    typedef void (CALLBACK *JsObjectBeforeCollectCallback)(_In_ JsRef ref, _In_opt_ void *callbackState);

    /// <summary>
    ///     A background work item callback.
    /// </summary>
    /// <remarks>
    ///     This is passed to the host's thread service (if provided) to allow the host to
    ///     invoke the work item callback on the background thread of its choice.
    /// </remarks>
    /// <param name="callbackState">Data argument passed to the thread service.</param>
    typedef void (CALLBACK *JsBackgroundWorkItemCallback)(_In_opt_ void *callbackState);

    /// <summary>
    ///     A thread service callback.
    /// </summary>
    /// <remarks>
    ///     The host can specify a background thread service when calling <c>JsCreateRuntime</c>. If
    ///     specified, then background work items will be passed to the host using this callback. The
    ///     host is expected to either begin executing the background work item immediately and return
    ///     true or return false and the runtime will handle the work item in-thread.
    /// </remarks>
    /// <param name="callback">The callback for the background work item.</param>
    /// <param name="callbackState">The data argument to be passed to the callback.</param>
    typedef bool (CALLBACK *JsThreadServiceCallback)(_In_ JsBackgroundWorkItemCallback callback, _In_opt_ void *callbackState);

    /// <summary>
    ///     Creates a new runtime.
    /// </summary>
    /// <param name="attributes">The attributes of the runtime to be created.</param>
    /// <param name="threadService">The thread service for the runtime. Can be null.</param>
    /// <param name="runtime">The runtime created.</param>
    /// <remarks>In the edge-mode binary, chakra.dll, this function lacks the <c>runtimeVersion</c>
    /// parameter (compare to jsrt9.h).</remarks>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsCreateRuntime(
            _In_ JsRuntimeAttributes attributes,
            _In_opt_ JsThreadServiceCallback threadService,
            _Out_ JsRuntimeHandle *runtime);

    /// <summary>
    ///     Performs a full garbage collection.
    /// </summary>
    /// <param name="runtime">The runtime in which the garbage collection will be performed.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsCollectGarbage(
            _In_ JsRuntimeHandle runtime);

    /// <summary>
    ///     Disposes a runtime.
    /// </summary>
    /// <remarks>
    ///     Once a runtime has been disposed, all resources owned by it are invalid and cannot be used.
    ///     If the runtime is active (i.e. it is set to be current on a particular thread), it cannot
    ///     be disposed.
    /// </remarks>
    /// <param name="runtime">The runtime to dispose.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsDisposeRuntime(
            _In_ JsRuntimeHandle runtime);

    /// <summary>
    ///     Gets the current memory usage for a runtime.
    /// </summary>
    /// <remarks>
    ///     Memory usage can be always be retrieved, regardless of whether or not the runtime is active
    ///     on another thread.
    /// </remarks>
    /// <param name="runtime">The runtime whose memory usage is to be retrieved.</param>
    /// <param name="memoryUsage">The runtime's current memory usage, in bytes.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetRuntimeMemoryUsage(
            _In_ JsRuntimeHandle runtime,
            _Out_ size_t *memoryUsage);

    /// <summary>
    ///     Gets the current memory limit for a runtime.
    /// </summary>
    /// <remarks>
    ///     The memory limit of a runtime can be always be retrieved, regardless of whether or not the
    ///     runtime is active on another thread.
    /// </remarks>
    /// <param name="runtime">The runtime whose memory limit is to be retrieved.</param>
    /// <param name="memoryLimit">
    ///     The runtime's current memory limit, in bytes, or -1 if no limit has been set.
    /// </param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetRuntimeMemoryLimit(
            _In_ JsRuntimeHandle runtime,
            _Out_ size_t *memoryLimit);

    /// <summary>
    ///     Sets the current memory limit for a runtime.
    /// </summary>
    /// <remarks>
    ///     <para>
    ///     A memory limit will cause any operation which exceeds the limit to fail with an "out of
    ///     memory" error. Setting a runtime's memory limit to -1 means that the runtime has no memory
    ///     limit. New runtimes  default to having no memory limit. If the new memory limit exceeds
    ///     current usage, the call will succeed and any future allocations in this runtime will fail
    ///     until the runtime's memory usage drops below the limit.
    ///     </para>
    ///     <para>
    ///     A runtime's memory limit can be always be set, regardless of whether or not the runtime is
    ///     active on another thread.
    ///     </para>
    /// </remarks>
    /// <param name="runtime">The runtime whose memory limit is to be set.</param>
    /// <param name="memoryLimit">
    ///     The new runtime memory limit, in bytes, or -1 for no memory limit.
    /// </param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsSetRuntimeMemoryLimit(
            _In_ JsRuntimeHandle runtime,
            _In_ size_t memoryLimit);

    /// <summary>
    ///     Sets a memory allocation callback for specified runtime
    /// </summary>
    /// <remarks>
    ///     <para>
    ///     Registering a memory allocation callback will cause the runtime to call back to the host
    ///     whenever it acquires memory from, or releases memory to, the OS. The callback routine is
    ///     called before the runtime memory manager allocates a block of memory. The allocation will
    ///     be rejected if the callback returns false. The runtime memory manager will also invoke the
    ///     callback routine after freeing a block of memory, as well as after allocation failures.
    ///     </para>
    ///     <para>
    ///     The callback is invoked on the current runtime execution thread, therefore execution is
    ///     blocked until the callback completes.
    ///     </para>
    ///     <para>
    ///     The return value of the callback is not stored; previously rejected allocations will not
    ///     prevent the runtime from invoking the callback again later for new memory allocations.
    ///     </para>
    /// </remarks>
    /// <param name="runtime">The runtime for which to register the allocation callback.</param>
    /// <param name="callbackState">
    ///     User provided state that will be passed back to the callback.
    /// </param>
    /// <param name="allocationCallback">
    ///     Memory allocation callback to be called for memory allocation events.
    /// </param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsSetRuntimeMemoryAllocationCallback(
            _In_ JsRuntimeHandle runtime,
            _In_opt_ void *callbackState,
            _In_ JsMemoryAllocationCallback allocationCallback);

    /// <summary>
    ///     Sets a callback function that is called by the runtime before garbage collection.
    /// </summary>
    /// <remarks>
    ///     <para>
    ///     The callback is invoked on the current runtime execution thread, therefore execution is
    ///     blocked until the callback completes.
    ///     </para>
    ///     <para>
    ///     The callback can be used by hosts to prepare for garbage collection. For example, by
    ///     releasing unnecessary references on Chakra objects.
    ///     </para>
    /// </remarks>
    /// <param name="runtime">The runtime for which to register the allocation callback.</param>
    /// <param name="callbackState">
    ///     User provided state that will be passed back to the callback.
    /// </param>
    /// <param name="beforeCollectCallback">The callback function being set.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsSetRuntimeBeforeCollectCallback(
            _In_ JsRuntimeHandle runtime,
            _In_opt_ void *callbackState,
            _In_ JsBeforeCollectCallback beforeCollectCallback);

    /// <summary>
    ///     Adds a reference to a garbage collected object.
    /// </summary>
    /// <remarks>
    ///     This only needs to be called on <c>JsRef</c> handles that are not going to be stored
    ///     somewhere on the stack. Calling <c>JsAddRef</c> ensures that the object the <c>JsRef</c>
    ///     refers to will not be freed until <c>JsRelease</c> is called.
    /// </remarks>
    /// <param name="ref">The object to add a reference to.</param>
    /// <param name="count">The object's new reference count (can pass in null).</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsAddRef(
            _In_ JsRef ref,
            _Out_opt_ unsigned int *count);

    /// <summary>
    ///     Releases a reference to a garbage collected object.
    /// </summary>
    /// <remarks>
    ///     Removes a reference to a <c>JsRef</c> handle that was created by <c>JsAddRef</c>.
    /// </remarks>
    /// <param name="ref">The object to add a reference to.</param>
    /// <param name="count">The object's new reference count (can pass in null).</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsRelease(
            _In_ JsRef ref,
            _Out_opt_ unsigned int *count);

    /// <summary>
    ///     Sets a callback function that is called by the runtime before garbage collection of
    ///     an object.
    /// </summary>
    /// <remarks>
    ///     <para>
    ///     The callback is invoked on the current runtime execution thread, therefore execution is
    ///     blocked until the callback completes.
    ///     </para>
    /// </remarks>
    /// <param name="ref">The object for which to register the callback.</param>
    /// <param name="callbackState">
    ///     User provided state that will be passed back to the callback.
    /// </param>
    /// <param name="objectBeforeCollectCallback">The callback function being set. Use null to clear
    ///     previously registered callback.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsSetObjectBeforeCollectCallback(
            _In_ JsRef ref,
            _In_opt_ void *callbackState,
            _In_ JsObjectBeforeCollectCallback objectBeforeCollectCallback);

    /// <summary>
    ///     Creates a script context for running scripts.
    /// </summary>
    /// <remarks>
    ///     Each script context has its own global object that is isolated from all other script
    ///     contexts.
    /// </remarks>
    /// <param name="runtime">The runtime the script context is being created in.</param>
    /// <param name="newContext">The created script context.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsCreateContext(
            _In_ JsRuntimeHandle runtime,
            _Out_ JsContextRef *newContext);

    /// <summary>
    ///     Gets the current script context on the thread.
    /// </summary>
    /// <param name="currentContext">
    ///     The current script context on the thread, null if there is no current script context.
    /// </param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetCurrentContext(
            _Out_ JsContextRef *currentContext);

    /// <summary>
    ///     Sets the current script context on the thread.
    /// </summary>
    /// <param name="context">The script context to make current.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsSetCurrentContext(
            _In_ JsContextRef context);

    /// <summary>
    ///     Gets the script context that the object belongs to.
    /// </summary>
    /// <param name="object">The object to get the context from.</param>
    /// <param name="context">The context the object belongs to.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetContextOfObject(
            _In_ JsValueRef object,
            _Out_ JsContextRef *context);

    /// <summary>
    ///     Gets the internal data set on JsrtContext.
    /// </summary>
    /// <param name="context">The context to get the data from.</param>
    /// <param name="data">The pointer to the data where data will be returned.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetContextData(
            _In_ JsContextRef context,
            _Out_ void **data);

    /// <summary>
    ///     Sets the internal data of JsrtContext.
    /// </summary>
    /// <param name="context">The context to set the data to.</param>
    /// <param name="data">The pointer to the data to be set.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsSetContextData(
            _In_ JsContextRef context,
            _In_ void *data);

    /// <summary>
    ///     Gets the runtime that the context belongs to.
    /// </summary>
    /// <param name="context">The context to get the runtime from.</param>
    /// <param name="runtime">The runtime the context belongs to.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetRuntime(
            _In_ JsContextRef context,
            _Out_ JsRuntimeHandle *runtime);

    /// <summary>
    ///     Tells the runtime to do any idle processing it need to do.
    /// </summary>
    /// <remarks>
    ///     <para>
    ///     If idle processing has been enabled for the current runtime, calling <c>JsIdle</c> will
    ///     inform the current runtime that the host is idle and that the runtime can perform
    ///     memory cleanup tasks.
    ///     </para>
    ///     <para>
    ///     <c>JsIdle</c> can also return the number of system ticks until there will be more idle work
    ///     for the runtime to do. Calling <c>JsIdle</c> before this number of ticks has passed will do
    ///     no work.
    ///     </para>
    ///     <para>
    ///     Requires an active script context.
    ///     </para>
    /// </remarks>
    /// <param name="nextIdleTick">
    ///     The next system tick when there will be more idle work to do. Can be null. Returns the
    ///     maximum number of ticks if there no upcoming idle work to do.
    /// </param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsIdle(
            _Out_opt_ unsigned int *nextIdleTick);

    /// <summary>
    ///     Parses a script and returns a function representing the script.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="script">The script to parse.</param>
    /// <param name="sourceContext">
    ///     A cookie identifying the script that can be used by debuggable script contexts.
    /// </param>
    /// <param name="sourceUrl">The location the script came from.</param>
    /// <param name="result">A function representing the script code.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsParseScript(
            _In_z_ const wchar_t *script,
            _In_ JsSourceContext sourceContext,
            _In_z_ const wchar_t *sourceUrl,
            _Out_ JsValueRef *result);

    /// <summary>
    ///     Executes a script.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="script">The script to run.</param>
    /// <param name="sourceContext">
    ///     A cookie identifying the script that can be used by debuggable script contexts.
    /// </param>
    /// <param name="sourceUrl">The location the script came from.</param>
    /// <param name="result">The result of the script, if any. This parameter can be null.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsRunScript(
            _In_z_ const wchar_t *script,
            _In_ JsSourceContext sourceContext,
            _In_z_ const wchar_t *sourceUrl,
            _Out_ JsValueRef *result);

    /// <summary>
    ///     Serializes a parsed script to a buffer than can be reused.
    /// </summary>
    /// <remarks>
    ///     <para>
    ///     <c>JsSerializeScript</c> parses a script and then stores the parsed form of the script in a
    ///     runtime-independent format. The serialized script then can be deserialized in any
    ///     runtime without requiring the script to be re-parsed.
    ///     </para>
    ///     <para>
    ///     Requires an active script context.
    ///     </para>
    /// </remarks>
    /// <param name="script">The script to serialize.</param>
    /// <param name="buffer">The buffer to put the serialized script into. Can be null.</param>
    /// <param name="bufferSize">
    ///     On entry, the size of the buffer, in bytes; on exit, the size of the buffer, in bytes,
    ///     required to hold the serialized script.
    /// </param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsSerializeScript(
            _In_z_ const wchar_t *script,
            _Out_writes_to_opt_(*bufferSize, *bufferSize) BYTE *buffer,
            _Inout_ unsigned long *bufferSize);

    /// <summary>
    ///     Called by the runtime to load the source code of the serialized script.
    ///     The caller must keep the script buffer valid until the JsSerializedScriptUnloadCallback.
    /// </summary>
    /// <param name="sourceContext">The context passed to Js[Parse|Run]SerializedScriptWithCallback</param>
    /// <param name="scriptBuffer">The script returned.</param>
    /// <returns>
    ///     true if the operation succeeded, false otherwise.
    /// </returns>
    typedef bool (CALLBACK * JsSerializedScriptLoadSourceCallback)(_In_ JsSourceContext sourceContext, _Outptr_result_z_ const wchar_t** scriptBuffer);

    /// <summary>
    ///     Called by the runtime when it is finished with all resources related to the script execution.
    ///     The caller should free the source if loaded, the byte code, and the context at this time.
    /// </summary>
    /// <param name="sourceContext">The context passed to Js[Parse|Run]SerializedScriptWithCallback</param>
    typedef void (CALLBACK * JsSerializedScriptUnloadCallback)(_In_ JsSourceContext sourceContext);

    /// <summary>
    ///     Parses a serialized script and returns a function representing the script.
    ///     Provides the ability to lazy load the script source only if/when it is needed.
    /// </summary>
    /// <remarks>
    ///     <para>
    ///     Requires an active script context.
    ///     </para>
    ///     <para>
    ///     The runtime will hold on to the buffer until all instances of any functions created from
    ///     the buffer are garbage collected.  It will then call scriptUnloadCallback to inform the
    ///     caller it is safe to release.
    ///     </para>
    /// </remarks>
    /// <param name="scriptLoadCallback">Callback called when the source code of the script needs to be loaded.</param>
    /// <param name="scriptUnloadCallback">Callback called when the serialized script and source code are no longer needed.</param>
    /// <param name="buffer">The serialized script.</param>
    /// <param name="sourceContext">
    ///     A cookie identifying the script that can be used by debuggable script contexts.
    ///     This context will passed into scriptLoadCallback and scriptUnloadCallback.
    /// </param>
    /// <param name="sourceUrl">The location the script came from.</param>
    /// <param name="result">A function representing the script code.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsParseSerializedScriptWithCallback(
            _In_ JsSerializedScriptLoadSourceCallback scriptLoadCallback,
            _In_ JsSerializedScriptUnloadCallback scriptUnloadCallback,
            _In_ BYTE *buffer,
            _In_ JsSourceContext sourceContext,
            _In_z_ const wchar_t *sourceUrl,
            _Out_ JsValueRef * result);

    /// <summary>
    ///     Runs a serialized script.
    ///     Provides the ability to lazy load the script source only if/when it is needed.
    /// </summary>
    /// <remarks>
    ///     <para>
    ///     Requires an active script context.
    ///     </para>
    ///     <para>
    ///     The runtime will hold on to the buffer until all instances of any functions created from
    ///     the buffer are garbage collected.  It will then call scriptUnloadCallback to inform the
    ///     caller it is safe to release.
    ///     </para>
    /// </remarks>
    /// <param name="scriptLoadCallback">Callback called when the source code of the script needs to be loaded.</param>
    /// <param name="scriptUnloadCallback">Callback called when the serialized script and source code are no longer needed.</param>
    /// <param name="buffer">The serialized script.</param>
    /// <param name="sourceContext">
    ///     A cookie identifying the script that can be used by debuggable script contexts.
    ///     This context will passed into scriptLoadCallback and scriptUnloadCallback.
    /// </param>
    /// <param name="sourceUrl">The location the script came from.</param>
    /// <param name="result">
    ///     The result of running the script, if any. This parameter can be null.
    /// </param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsRunSerializedScriptWithCallback(
            _In_ JsSerializedScriptLoadSourceCallback scriptLoadCallback,
            _In_ JsSerializedScriptUnloadCallback scriptUnloadCallback,
            _In_ BYTE *buffer,
            _In_ JsSourceContext sourceContext,
            _In_z_ const wchar_t *sourceUrl,
            _Out_opt_ JsValueRef * result);

    /// <summary>
    ///     Parses a serialized script and returns a function representing the script.
    /// </summary>
    /// <remarks>
    ///     <para>
    ///     Requires an active script context.
    ///     </para>
    ///     <para>
    ///     The runtime will hold on to the buffer until all instances of any functions created from
    ///     the buffer are garbage collected.
    ///     </para>
    /// </remarks>
    /// <param name="script">The script to parse.</param>
    /// <param name="buffer">The serialized script.</param>
    /// <param name="sourceContext">
    ///     A cookie identifying the script that can be used by debuggable script contexts.
    /// </param>
    /// <param name="sourceUrl">The location the script came from.</param>
    /// <param name="result">A function representing the script code.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsParseSerializedScript(
            _In_z_ const wchar_t *script,
            _In_ BYTE *buffer,
            _In_ JsSourceContext sourceContext,
            _In_z_ const wchar_t *sourceUrl,
            _Out_ JsValueRef *result);

    /// <summary>
    ///     Runs a serialized script.
    /// </summary>
    /// <remarks>
    ///     <para>
    ///     Requires an active script context.
    ///     </para>
    ///     <para>
    ///     The runtime will hold on to the buffer until all instances of any functions created from
    ///     the buffer are garbage collected.
    ///     </para>
    /// </remarks>
    /// <param name="script">The source code of the serialized script.</param>
    /// <param name="buffer">The serialized script.</param>
    /// <param name="sourceContext">
    ///     A cookie identifying the script that can be used by debuggable script contexts.
    /// </param>
    /// <param name="sourceUrl">The location the script came from.</param>
    /// <param name="result">
    ///     The result of running the script, if any. This parameter can be null.
    /// </param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsRunSerializedScript(
            _In_z_ const wchar_t *script,
            _In_ BYTE *buffer,
            _In_ JsSourceContext sourceContext,
            _In_z_ const wchar_t *sourceUrl,
            _Out_ JsValueRef *result);

    /// <summary>
    ///     Gets the property ID associated with the name.
    /// </summary>
    /// <remarks>
    ///     <para>
    ///     Property IDs are specific to a context and cannot be used across contexts.
    ///     </para>
    ///     <para>
    ///     Requires an active script context.
    ///     </para>
    /// </remarks>
    /// <param name="name">
    ///     The name of the property ID to get or create. The name may consist of only digits.
    /// </param>
    /// <param name="propertyId">The property ID in this runtime for the given name.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetPropertyIdFromName(
            _In_z_ const wchar_t *name,
            _Out_ JsPropertyIdRef *propertyId);

    /// <summary>
    ///     Gets the name associated with the property ID.
    /// </summary>
    /// <remarks>
    ///     <para>
    ///     Requires an active script context.
    ///     </para>
    ///     <para>
    ///     The returned buffer is valid as long as the runtime is alive and cannot be used
    ///     once the runtime has been disposed.
    ///     </para>
    /// </remarks>
    /// <param name="propertyId">The property ID to get the name of.</param>
    /// <param name="name">The name associated with the property ID.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetPropertyNameFromId(
            _In_ JsPropertyIdRef propertyId,
            _Outptr_result_z_ const wchar_t **name);

    /// <summary>
    ///     Gets the symbol associated with the property ID.
    /// </summary>
    /// <remarks>
    ///     <para>
    ///     Requires an active script context.
    ///     </para>
    /// </remarks>
    /// <param name="propertyId">The property ID to get the symbol of.</param>
    /// <param name="symbol">The symbol associated with the property ID.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetSymbolFromPropertyId(
            _In_ JsPropertyIdRef propertyId,
            _Out_ JsValueRef *symbol);

    /// <summary>
    ///     Type enumeration of a JavaScript property
    /// </summary>
    typedef enum _JsPropertyIdType {
        /// <summary>
        ///     Type enumeration of a JavaScript string property
        /// </summary>
        JsPropertyIdTypeString,
        /// <summary>
        ///     Type enumeration of a JavaScript symbol property
        /// </summary>
        JsPropertyIdTypeSymbol
    } JsPropertyIdType;

    /// <summary>
    ///     Gets the type of property
    /// </summary>
    /// <remarks>
    ///     <para>
    ///     Requires an active script context.
    ///     </para>
    /// </remarks>
    /// <param name="propertyId">The property ID to get the type of.</param>
    /// <param name="propertyIdType">The JsPropertyIdType of the given property ID</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetPropertyIdType(
            _In_ JsPropertyIdRef propertyId,
            _Out_ JsPropertyIdType* propertyIdType);


    /// <summary>
    ///     Gets the property ID associated with the symbol.
    /// </summary>
    /// <remarks>
    ///     <para>
    ///     Property IDs are specific to a context and cannot be used across contexts.
    ///     </para>
    ///     <para>
    ///     Requires an active script context.
    ///     </para>
    /// </remarks>
    /// <param name="symbol">
    ///     The symbol whose property ID is being retrieved.
    /// </param>
    /// <param name="propertyId">The property ID for the given symbol.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetPropertyIdFromSymbol(
            _In_ JsValueRef symbol,
            _Out_ JsPropertyIdRef *propertyId);

    /// <summary>
    ///     Creates a Javascript symbol.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="description">The string description of the symbol. Can be null.</param>
    /// <param name="result">The new symbol.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsCreateSymbol(
            _In_ JsValueRef description,
            _Out_ JsValueRef *result);

    /// <summary>
    ///     Gets the list of all symbol properties on the object.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="object">The object from which to get the property symbols.</param>
    /// <param name="propertySymbols">An array of property symbols.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetOwnPropertySymbols(
            _In_ JsValueRef object,
            _Out_ JsValueRef *propertySymbols);

    /// <summary>
    ///     Gets the value of <c>undefined</c> in the current script context.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="undefinedValue">The <c>undefined</c> value.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetUndefinedValue(
            _Out_ JsValueRef *undefinedValue);

    /// <summary>
    ///     Gets the value of <c>null</c> in the current script context.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="nullValue">The <c>null</c> value.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetNullValue(
            _Out_ JsValueRef *nullValue);

    /// <summary>
    ///     Gets the value of <c>true</c> in the current script context.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="trueValue">The <c>true</c> value.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetTrueValue(
            _Out_ JsValueRef *trueValue);

    /// <summary>
    ///     Gets the value of <c>false</c> in the current script context.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="falseValue">The <c>false</c> value.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetFalseValue(
            _Out_ JsValueRef *falseValue);

    /// <summary>
    ///     Creates a Boolean value from a <c>bool</c> value.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="value">The value to be converted.</param>
    /// <param name="booleanValue">The converted value.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsBoolToBoolean(
            _In_ bool value,
            _Out_ JsValueRef *booleanValue);

    /// <summary>
    ///     Retrieves the <c>bool</c> value of a Boolean value.
    /// </summary>
    /// <param name="value">The value to be converted.</param>
    /// <param name="boolValue">The converted value.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsBooleanToBool(
            _In_ JsValueRef value,
            _Out_ bool *boolValue);

    /// <summary>
    ///     Converts the value to Boolean using standard JavaScript semantics.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="value">The value to be converted.</param>
    /// <param name="booleanValue">The converted value.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsConvertValueToBoolean(
            _In_ JsValueRef value,
            _Out_ JsValueRef *booleanValue);

    /// <summary>
    ///     The JavaScript type of a JsValueRef.
    /// </summary>
    typedef enum _JsValueType
    {
        /// <summary>
        ///     The value is the <c>undefined</c> value.
        /// </summary>
        JsUndefined = 0,
        /// <summary>
        ///     The value is the <c>null</c> value.
        /// </summary>
        JsNull = 1,
        /// <summary>
        ///     The value is a JavaScript number value.
        /// </summary>
        JsNumber = 2,
        /// <summary>
        ///     The value is a JavaScript string value.
        /// </summary>
        JsString = 3,
        /// <summary>
        ///     The value is a JavaScript Boolean value.
        /// </summary>
        JsBoolean = 4,
        /// <summary>
        ///     The value is a JavaScript object value.
        /// </summary>
        JsObject = 5,
        /// <summary>
        ///     The value is a JavaScript function object value.
        /// </summary>
        JsFunction = 6,
        /// <summary>
        ///     The value is a JavaScript error object value.
        /// </summary>
        JsError = 7,
        /// <summary>
        ///     The value is a JavaScript array object value.
        /// </summary>
        JsArray = 8,
        /// <summary>
        ///     The value is a JavaScript symbol value.
        /// </summary>
        JsSymbol = 9,
        /// <summary>
        ///     The value is a JavaScript ArrayBuffer object value.
        /// </summary>
        JsArrayBuffer = 10,
        /// <summary>
        ///     The value is a JavaScript typed array object value.
        /// </summary>
        JsTypedArray = 11,
        /// <summary>
        ///     The value is a JavaScript DataView object value.
        /// </summary>
        JsDataView = 12,
    } JsValueType;

    /// <summary>
    ///     Gets the JavaScript type of a JsValueRef.
    /// </summary>
    /// <param name="value">The value whose type is to be returned.</param>
    /// <param name="type">The type of the value.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetValueType(
            _In_ JsValueRef value,
            _Out_ JsValueType *type);

    /// <summary>
    ///     Creates a number value from a <c>double</c> value.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="doubleValue">The <c>double</c> to convert to a number value.</param>
    /// <param name="value">The new number value.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsDoubleToNumber(
            _In_ double doubleValue,
            _Out_ JsValueRef *value);

    /// <summary>
    ///     Creates a number value from an <c>int</c> value.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="intValue">The <c>int</c> to convert to a number value.</param>
    /// <param name="value">The new number value.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsIntToNumber(
            _In_ int intValue,
            _Out_ JsValueRef *value);

    /// <summary>
    ///     Retrieves the <c>double</c> value of a number value.
    /// </summary>
    /// <remarks>
    ///     This function retrieves the value of a number value. It will fail with
    ///     <c>JsErrorInvalidArgument</c> if the type of the value is not number.
    /// </remarks>
    /// <param name="value">The number value to convert to a <c>double</c> value.</param>
    /// <param name="doubleValue">The <c>double</c> value.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsNumberToDouble(
            _In_ JsValueRef value,
            _Out_ double *doubleValue);

    /// <summary>
    ///     Retrieves the <c>int</c> value of a number value.
    /// </summary>
    /// <remarks>
    ///     This function retrieves the value of a number value and converts to an <c>int</c> value.
    ///     It will fail with <c>JsErrorInvalidArgument</c> if the type of the value is not number.
    /// </remarks>
    /// <param name="value">The number value to convert to an <c>int</c> value.</param>
    /// <param name="intValue">The <c>int</c> value.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsNumberToInt(
            _In_ JsValueRef value,
            _Out_ int *intValue);

    /// <summary>
    ///     Converts the value to number using standard JavaScript semantics.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="value">The value to be converted.</param>
    /// <param name="numberValue">The converted value.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsConvertValueToNumber(
            _In_ JsValueRef value,
            _Out_ JsValueRef *numberValue);

    /// <summary>
    ///     Gets the length of a string value.
    /// </summary>
    /// <param name="stringValue">The string value to get the length of.</param>
    /// <param name="length">The length of the string.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetStringLength(
            _In_ JsValueRef stringValue,
            _Out_ int *length);

    /// <summary>
    ///     Creates a string value from a string pointer.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="stringValue">The string pointer to convert to a string value.</param>
    /// <param name="stringLength">The length of the string to convert.</param>
    /// <param name="value">The new string value.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsPointerToString(
            _In_reads_(stringLength) const wchar_t *stringValue,
            _In_ size_t stringLength,
            _Out_ JsValueRef *value);

    /// <summary>
    ///     Retrieves the string pointer of a string value.
    /// </summary>
    /// <remarks>
    ///     <para>
    ///     This function retrieves the string pointer of a string value. It will fail with
    ///     <c>JsErrorInvalidArgument</c> if the type of the value is not string. The lifetime
    ///     of the string returned will be the same as the lifetime of the value it came from, however
    ///     the string pointer is not considered a reference to the value (and so will not keep it
    ///     from being collected).
    ///     </para>
    ///     <para>
    ///     Requires an active script context.
    ///     </para>
    /// </remarks>
    /// <param name="value">The string value to convert to a string pointer.</param>
    /// <param name="stringValue">The string pointer.</param>
    /// <param name="stringLength">The length of the string.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsStringToPointer(
            _In_ JsValueRef value,
            _Outptr_result_buffer_(*stringLength) const wchar_t **stringValue,
            _Out_ size_t *stringLength);

    /// <summary>
    ///     Converts the value to string using standard JavaScript semantics.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="value">The value to be converted.</param>
    /// <param name="stringValue">The converted value.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsConvertValueToString(
            _In_ JsValueRef value,
            _Out_ JsValueRef *stringValue);

    /// <summary>
    ///     Gets the global object in the current script context.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="globalObject">The global object.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetGlobalObject(
            _Out_ JsValueRef *globalObject);

    /// <summary>
    ///     Creates a new object.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="object">The new object.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsCreateObject(
            _Out_ JsValueRef *object);

    /// <summary>
    ///     A finalizer callback.
    /// </summary>
    /// <param name="data">
    ///     The external data that was passed in when creating the object being finalized.
    /// </param>
    typedef void (CALLBACK *JsFinalizeCallback)(_In_opt_ void *data);

    /// <summary>
    ///     Creates a new object that stores some external data.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="data">External data that the object will represent. May be null.</param>
    /// <param name="finalizeCallback">
    ///     A callback for when the object is finalized. May be null.
    /// </param>
    /// <param name="object">The new object.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsCreateExternalObject(
            _In_opt_ void *data,
            _In_opt_ JsFinalizeCallback finalizeCallback,
            _Out_ JsValueRef *object);

    /// <summary>
    ///     Converts the value to object using standard JavaScript semantics.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="value">The value to be converted.</param>
    /// <param name="object">The converted value.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsConvertValueToObject(
            _In_ JsValueRef value,
            _Out_ JsValueRef *object);

    /// <summary>
    ///     Returns the prototype of an object.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="object">The object whose prototype is to be returned.</param>
    /// <param name="prototypeObject">The object's prototype.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetPrototype(
            _In_ JsValueRef object,
            _Out_ JsValueRef *prototypeObject);

    /// <summary>
    ///     Sets the prototype of an object.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="object">The object whose prototype is to be changed.</param>
    /// <param name="prototypeObject">The object's new prototype.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsSetPrototype(
            _In_ JsValueRef object,
            _In_ JsValueRef prototypeObject);

    /// <summary>
    ///     Performs JavaScript "instanceof" operator test.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="object">The object to test.</param>
    /// <param name="constructor">The constructor function to test against.</param>
    /// <param name="result">Whether "object instanceof constructor" is true.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsInstanceOf(
            _In_ JsValueRef object,
            _In_ JsValueRef constructor,
            _Out_ bool *result);

    /// <summary>
    ///     Returns a value that indicates whether an object is extensible or not.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="object">The object to test.</param>
    /// <param name="value">Whether the object is extensible or not.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetExtensionAllowed(
            _In_ JsValueRef object,
            _Out_ bool *value);

    /// <summary>
    ///     Makes an object non-extensible.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="object">The object to make non-extensible.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsPreventExtension(
            _In_ JsValueRef object);

    /// <summary>
    ///     Gets an object's property.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="object">The object that contains the property.</param>
    /// <param name="propertyId">The ID of the property.</param>
    /// <param name="value">The value of the property.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetProperty(
            _In_ JsValueRef object,
            _In_ JsPropertyIdRef propertyId,
            _Out_ JsValueRef *value);

    /// <summary>
    ///     Gets a property descriptor for an object's own property.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="object">The object that has the property.</param>
    /// <param name="propertyId">The ID of the property.</param>
    /// <param name="propertyDescriptor">The property descriptor.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetOwnPropertyDescriptor(
            _In_ JsValueRef object,
            _In_ JsPropertyIdRef propertyId,
            _Out_ JsValueRef *propertyDescriptor);

    /// <summary>
    ///     Gets the list of all properties on the object.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="object">The object from which to get the property names.</param>
    /// <param name="propertyNames">An array of property names.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetOwnPropertyNames(
            _In_ JsValueRef object,
            _Out_ JsValueRef *propertyNames);

    /// <summary>
    ///     Puts an object's property.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="object">The object that contains the property.</param>
    /// <param name="propertyId">The ID of the property.</param>
    /// <param name="value">The new value of the property.</param>
    /// <param name="useStrictRules">The property set should follow strict mode rules.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsSetProperty(
            _In_ JsValueRef object,
            _In_ JsPropertyIdRef propertyId,
            _In_ JsValueRef value,
            _In_ bool useStrictRules);

    /// <summary>
    ///     Determines whether an object has a property.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="object">The object that may contain the property.</param>
    /// <param name="propertyId">The ID of the property.</param>
    /// <param name="hasProperty">Whether the object (or a prototype) has the property.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsHasProperty(
            _In_ JsValueRef object,
            _In_ JsPropertyIdRef propertyId,
            _Out_ bool *hasProperty);

    /// <summary>
    ///     Deletes an object's property.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="object">The object that contains the property.</param>
    /// <param name="propertyId">The ID of the property.</param>
    /// <param name="useStrictRules">The property set should follow strict mode rules.</param>
    /// <param name="result">Whether the property was deleted.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsDeleteProperty(
            _In_ JsValueRef object,
            _In_ JsPropertyIdRef propertyId,
            _In_ bool useStrictRules,
            _Out_ JsValueRef *result);

    /// <summary>
    ///     Defines a new object's own property from a property descriptor.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="object">The object that has the property.</param>
    /// <param name="propertyId">The ID of the property.</param>
    /// <param name="propertyDescriptor">The property descriptor.</param>
    /// <param name="result">Whether the property was defined.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsDefineProperty(
            _In_ JsValueRef object,
            _In_ JsPropertyIdRef propertyId,
            _In_ JsValueRef propertyDescriptor,
            _Out_ bool *result);

    /// <summary>
    ///     Tests whether an object has a value at the specified index.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="object">The object to operate on.</param>
    /// <param name="index">The index to test.</param>
    /// <param name="result">Whether the object has an value at the specified index.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsHasIndexedProperty(
            _In_ JsValueRef object,
            _In_ JsValueRef index,
            _Out_ bool *result);

    /// <summary>
    ///     Retrieve the value at the specified index of an object.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="object">The object to operate on.</param>
    /// <param name="index">The index to retrieve.</param>
    /// <param name="result">The retrieved value.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetIndexedProperty(
            _In_ JsValueRef object,
            _In_ JsValueRef index,
            _Out_ JsValueRef *result);

    /// <summary>
    ///     Set the value at the specified index of an object.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="object">The object to operate on.</param>
    /// <param name="index">The index to set.</param>
    /// <param name="value">The value to set.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsSetIndexedProperty(
            _In_ JsValueRef object,
            _In_ JsValueRef index,
            _In_ JsValueRef value);

    /// <summary>
    ///     Delete the value at the specified index of an object.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="object">The object to operate on.</param>
    /// <param name="index">The index to delete.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsDeleteIndexedProperty(
            _In_ JsValueRef object,
            _In_ JsValueRef index);

    /// <summary>
    ///     Determines whether an object has its indexed properties in external data.
    /// </summary>
    /// <param name="object">The object.</param>
    /// <param name="value">Whether the object has its indexed properties in external data.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsHasIndexedPropertiesExternalData(
            _In_ JsValueRef object,
            _Out_ bool* value);

    /// <summary>
    ///     Retrieves an object's indexed properties external data information.
    /// </summary>
    /// <param name="object">The object.</param>
    /// <param name="data">The external data back store for the object's indexed properties.</param>
    /// <param name="arrayType">The array element type in external data.</param>
    /// <param name="elementLength">The number of array elements in external data.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetIndexedPropertiesExternalData(
            _In_ JsValueRef object,
            _Out_ void** data,
            _Out_ JsTypedArrayType* arrayType,
            _Out_ unsigned int* elementLength);

    /// <summary>
    ///     Sets an object's indexed properties to external data. The external data will be used as back
    ///     store for the object's indexed properties and accessed like a typed array.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="object">The object to operate on.</param>
    /// <param name="data">The external data to be used as back store for the object's indexed properties.</param>
    /// <param name="arrayType">The array element type in external data.</param>
    /// <param name="elementLength">The number of array elements in external data.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsSetIndexedPropertiesToExternalData(
            _In_ JsValueRef object,
            _In_ void* data,
            _In_ JsTypedArrayType arrayType,
            _In_ unsigned int elementLength);

    /// <summary>
    ///     Compare two JavaScript values for equality.
    /// </summary>
    /// <remarks>
    ///     <para>
    ///     This function is equivalent to the <c>==</c> operator in Javascript.
    ///     </para>
    ///     <para>
    ///     Requires an active script context.
    ///     </para>
    /// </remarks>
    /// <param name="object1">The first object to compare.</param>
    /// <param name="object2">The second object to compare.</param>
    /// <param name="result">Whether the values are equal.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsEquals(
            _In_ JsValueRef object1,
            _In_ JsValueRef object2,
            _Out_ bool *result);

    /// <summary>
    ///     Compare two JavaScript values for strict equality.
    /// </summary>
    /// <remarks>
    ///     <para>
    ///     This function is equivalent to the <c>===</c> operator in Javascript.
    ///     </para>
    ///     <para>
    ///     Requires an active script context.
    ///     </para>
    /// </remarks>
    /// <param name="object1">The first object to compare.</param>
    /// <param name="object2">The second object to compare.</param>
    /// <param name="result">Whether the values are strictly equal.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsStrictEquals(
            _In_ JsValueRef object1,
            _In_ JsValueRef object2,
            _Out_ bool *result);

    /// <summary>
    ///     Determines whether an object is an external object.
    /// </summary>
    /// <param name="object">The object.</param>
    /// <param name="value">Whether the object is an external object.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsHasExternalData(
            _In_ JsValueRef object,
            _Out_ bool *value);

    /// <summary>
    ///     Retrieves the data from an external object.
    /// </summary>
    /// <param name="object">The external object.</param>
    /// <param name="externalData">
    ///     The external data stored in the object. Can be null if no external data is stored in the
    ///     object.
    /// </param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetExternalData(
            _In_ JsValueRef object,
            _Out_ void **externalData);

    /// <summary>
    ///     Sets the external data on an external object.
    /// </summary>
    /// <param name="object">The external object.</param>
    /// <param name="externalData">
    ///     The external data to be stored in the object. Can be null if no external data is
    ///     to be stored in the object.
    /// </param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsSetExternalData(
            _In_ JsValueRef object,
            _In_opt_ void *externalData);

    /// <summary>
    ///     Creates a Javascript array object.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="length">The initial length of the array.</param>
    /// <param name="result">The new array object.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsCreateArray(
            _In_ unsigned int length,
            _Out_ JsValueRef *result);

    /// <summary>
    ///     Creates a Javascript ArrayBuffer object.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="byteLength">
    ///     The number of bytes in the ArrayBuffer.
    /// </param>
    /// <param name="result">The new ArrayBuffer object.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsCreateArrayBuffer(
            _In_ unsigned int byteLength,
            _Out_ JsValueRef *result);

    /// <summary>
    ///     Creates a Javascript ArrayBuffer object to access external memory.
    /// </summary>
    /// <remarks>Requires an active script context.</remarks>
    /// <param name="data">A pointer to the external memory.</param>
    /// <param name="byteLength">The number of bytes in the external memory.</param>
    /// <param name="finalizeCallback">A callback for when the object is finalized. May be null.</param>
    /// <param name="callbackState">User provided state that will be passed back to finalizeCallback.</param>
    /// <param name="result">The new ArrayBuffer object.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsCreateExternalArrayBuffer(
            _Pre_maybenull_ _Pre_writable_byte_size_(byteLength) void *data,
            _In_ unsigned int byteLength,
            _In_opt_ JsFinalizeCallback finalizeCallback,
            _In_opt_ void *callbackState,
            _Out_ JsValueRef *result);

    /// <summary>
    ///     Creates a Javascript typed array object.
    /// </summary>
    /// <remarks>
    ///     <para>
    ///     The <c>baseArray</c> can be an <c>ArrayBuffer</c>, another typed array, or a JavaScript
    ///     <c>Array</c>. The returned typed array will use the baseArray if it is an ArrayBuffer, or
    ///     otherwise create and use a copy of the underlying source array.
    ///     </para>
    ///     <para>
    ///     Requires an active script context.
    ///     </para>
    /// </remarks>
    /// <param name="arrayType">The type of the array to create.</param>
    /// <param name="baseArray">
    ///     The base array of the new array. Use <c>JS_INVALID_REFERENCE</c> if no base array.
    /// </param>
    /// <param name="byteOffset">
    ///     The offset in bytes from the start of baseArray (ArrayBuffer) for result typed array to reference.
    ///     Only applicable when baseArray is an ArrayBuffer object. Must be 0 otherwise.
    /// </param>
    /// <param name="elementLength">
    ///     The number of elements in the array. Only applicable when creating a new typed array without
    ///     baseArray (baseArray is <c>JS_INVALID_REFERENCE</c>) or when baseArray is an ArrayBuffer object.
    ///     Must be 0 otherwise.
    /// </param>
    /// <param name="result">The new typed array object.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsCreateTypedArray(
            _In_ JsTypedArrayType arrayType,
            _In_ JsValueRef baseArray,
            _In_ unsigned int byteOffset,
            _In_ unsigned int elementLength,
            _Out_ JsValueRef *result);

    /// <summary>
    ///     Creates a Javascript DataView object.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="arrayBuffer">
    ///     An existing ArrayBuffer object to use as the storage for the result DataView object.
    /// </param>
    /// <param name="byteOffset">
    ///     The offset in bytes from the start of arrayBuffer for result DataView to reference.
    /// </param>
    /// <param name="byteLength">
    ///     The number of bytes in the ArrayBuffer for result DataView to reference.
    /// </param>
    /// <param name="result">The new DataView object.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsCreateDataView(
            _In_ JsValueRef arrayBuffer,
            _In_ unsigned int byteOffset,
            _In_ unsigned int byteLength,
            _Out_ JsValueRef *result);

    /// <summary>
    ///     Obtains frequently used properties of a typed array.
    /// </summary>
    /// <param name="typedArray">The typed array instance.</param>
    /// <param name="arrayType">The type of the array.</param>
    /// <param name="arrayBuffer">The ArrayBuffer backstore of the array.</param>
    /// <param name="byteOffset">The offset in bytes from the start of arrayBuffer referenced by the array.</param>
    /// <param name="byteLength">The number of bytes in the array.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetTypedArrayInfo(
            _In_ JsValueRef typedArray,
            _Out_opt_ JsTypedArrayType *arrayType,
            _Out_opt_ JsValueRef *arrayBuffer,
            _Out_opt_ unsigned int *byteOffset,
            _Out_opt_ unsigned int *byteLength);

    /// <summary>
    ///     Obtains the underlying memory storage used by an <c>ArrayBuffer</c>.
    /// </summary>
    /// <param name="arrayBuffer">The ArrayBuffer instance.</param>
    /// <param name="buffer">
    ///     The ArrayBuffer's buffer. The lifetime of the buffer returned is the same as the lifetime of the
    ///     the ArrayBuffer. The buffer pointer does not count as a reference to the ArrayBuffer for the purpose
    ///     of garbage collection.
    /// </param>
    /// <param name="bufferLength">The number of bytes in the buffer.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetArrayBufferStorage(
            _In_ JsValueRef arrayBuffer,
            _Outptr_result_bytebuffer_(*bufferLength) BYTE **buffer,
            _Out_ unsigned int *bufferLength);

    /// <summary>
    ///     Obtains the underlying memory storage used by a typed array.
    /// </summary>
    /// <param name="typedArray">The typed array instance.</param>
    /// <param name="buffer">
    ///     The array's buffer. The lifetime of the buffer returned is the same as the lifetime of the
    ///     the array. The buffer pointer does not count as a reference to the array for the purpose
    ///     of garbage collection.
    /// </param>
    /// <param name="bufferLength">The number of bytes in the buffer.</param>
    /// <param name="arrayType">The type of the array.</param>
    /// <param name="elementSize">
    ///     The size of an element of the array.
    /// </param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetTypedArrayStorage(
            _In_ JsValueRef typedArray,
            _Outptr_result_bytebuffer_(*bufferLength) BYTE **buffer,
            _Out_ unsigned int *bufferLength,
            _Out_opt_ JsTypedArrayType *arrayType,
            _Out_opt_ int *elementSize);

    /// <summary>
    ///     Obtains the underlying memory storage used by a DataView.
    /// </summary>
    /// <param name="dataView">The DataView instance.</param>
    /// <param name="buffer">
    ///     The DataView's buffer. The lifetime of the buffer returned is the same as the lifetime of the
    ///     the DataView. The buffer pointer does not count as a reference to the DataView for the purpose
    ///     of garbage collection.
    /// </param>
    /// <param name="bufferLength">The number of bytes in the buffer.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetDataViewStorage(
            _In_ JsValueRef dataView,
            _Outptr_result_bytebuffer_(*bufferLength) BYTE **buffer,
            _Out_ unsigned int *bufferLength);

    /// <summary>
    ///     Invokes a function.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="function">The function to invoke.</param>
    /// <param name="arguments">The arguments to the call.</param>
    /// <param name="argumentCount">The number of arguments being passed in to the function.</param>
    /// <param name="result">The value returned from the function invocation, if any.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsCallFunction(
            _In_ JsValueRef function,
            _In_reads_(argumentCount) JsValueRef *arguments,
            _In_ unsigned short argumentCount,
            _Out_opt_ JsValueRef *result);

    /// <summary>
    ///     Invokes a function as a constructor.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="function">The function to invoke as a constructor.</param>
    /// <param name="arguments">The arguments to the call.</param>
    /// <param name="argumentCount">The number of arguments being passed in to the function.</param>
    /// <param name="result">The value returned from the function invocation.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsConstructObject(
            _In_ JsValueRef function,
            _In_reads_(argumentCount) JsValueRef *arguments,
            _In_ unsigned short argumentCount,
            _Out_ JsValueRef *result);

    /// <summary>
    ///     A function callback.
    /// </summary>
    /// <param name="callee">
    ///     A function object that represents the function being invoked.
    /// </param>
    /// <param name="isConstructCall">Indicates whether this is a regular call or a 'new' call.</param>
    /// <param name="arguments">The arguments to the call.</param>
    /// <param name="argumentCount">The number of arguments.</param>
    /// <param name="callbackState">
    ///     The state passed to <c>JsCreateFunction</c>.
    /// </param>
    /// <returns>The result of the call, if any.</returns>
    typedef _Ret_maybenull_ JsValueRef(CALLBACK * JsNativeFunction)(_In_ JsValueRef callee, _In_ bool isConstructCall, _In_ JsValueRef *arguments, _In_ unsigned short argumentCount, _In_opt_ void *callbackState);

    /// <summary>
    ///     Creates a new JavaScript function.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="nativeFunction">The method to call when the function is invoked.</param>
    /// <param name="callbackState">
    ///     User provided state that will be passed back to the callback.
    /// </param>
    /// <param name="function">The new function object.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsCreateFunction(
            _In_ JsNativeFunction nativeFunction,
            _In_opt_ void *callbackState,
            _Out_ JsValueRef *function);

    /// <summary>
    ///     Creates a new JavaScript function with name.
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="name">The name of this function that will be used for diagnostics and stringification purposes.</param>
    /// <param name="nativeFunction">The method to call when the function is invoked.</param>
    /// <param name="callbackState">
    ///     User provided state that will be passed back to the callback.
    /// </param>
    /// <param name="function">The new function object.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsCreateNamedFunction(
            _In_ JsValueRef name,
            _In_ JsNativeFunction nativeFunction,
            _In_opt_ void *callbackState,
            _Out_ JsValueRef *function);

    /// <summary>
    ///     Creates a new JavaScript error object
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="message">Message for the error object.</param>
    /// <param name="error">The new error object.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsCreateError(
            _In_ JsValueRef message,
            _Out_ JsValueRef *error);

    /// <summary>
    ///     Creates a new JavaScript RangeError error object
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="message">Message for the error object.</param>
    /// <param name="error">The new error object.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsCreateRangeError(
            _In_ JsValueRef message,
            _Out_ JsValueRef *error);

    /// <summary>
    ///     Creates a new JavaScript ReferenceError error object
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="message">Message for the error object.</param>
    /// <param name="error">The new error object.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsCreateReferenceError(
            _In_ JsValueRef message,
            _Out_ JsValueRef *error);

    /// <summary>
    ///     Creates a new JavaScript SyntaxError error object
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="message">Message for the error object.</param>
    /// <param name="error">The new error object.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsCreateSyntaxError(
            _In_ JsValueRef message,
            _Out_ JsValueRef *error);

    /// <summary>
    ///     Creates a new JavaScript TypeError error object
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="message">Message for the error object.</param>
    /// <param name="error">The new error object.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsCreateTypeError(
            _In_ JsValueRef message,
            _Out_ JsValueRef *error);

    /// <summary>
    ///     Creates a new JavaScript URIError error object
    /// </summary>
    /// <remarks>
    ///     Requires an active script context.
    /// </remarks>
    /// <param name="message">Message for the error object.</param>
    /// <param name="error">The new error object.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsCreateURIError(
            _In_ JsValueRef message,
            _Out_ JsValueRef *error);

    /// <summary>
    ///     Determines whether the runtime of the current context is in an exception state.
    /// </summary>
    /// <remarks>
    ///     <para>
    ///     If a call into the runtime results in an exception (either as the result of running a
    ///     script or due to something like a conversion failure), the runtime is placed into an
    ///     "exception state." All calls into any context created by the runtime (except for the
    ///     exception APIs) will fail with <c>JsErrorInExceptionState</c> until the exception is
    ///     cleared.
    ///     </para>
    ///     <para>
    ///     If the runtime of the current context is in the exception state when a callback returns
    ///     into the engine, the engine will automatically rethrow the exception.
    ///     </para>
    ///     <para>
    ///     Requires an active script context.
    ///     </para>
    /// </remarks>
    /// <param name="hasException">
    ///     Whether the runtime of the current context is in the exception state.
    /// </param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsHasException(
            _Out_ bool *hasException);

    /// <summary>
    ///     Returns the exception that caused the runtime of the current context to be in the
    ///     exception state and resets the exception state for that runtime.
    /// </summary>
    /// <remarks>
    ///     <para>
    ///     If the runtime of the current context is not in an exception state, this API will return
    ///     <c>JsErrorInvalidArgument</c>. If the runtime is disabled, this will return an exception
    ///     indicating that the script was terminated, but it will not clear the exception (the
    ///     exception will be cleared if the runtime is re-enabled using
    ///     <c>JsEnableRuntimeExecution</c>).
    ///     </para>
    ///     <para>
    ///     Requires an active script context.
    ///     </para>
    /// </remarks>
    /// <param name="exception">The exception for the runtime of the current context.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsGetAndClearException(
            _Out_ JsValueRef *exception);

    /// <summary>
    ///     Sets the runtime of the current context to an exception state.
    /// </summary>
    /// <remarks>
    ///     <para>
    ///     If the runtime of the current context is already in an exception state, this API will
    ///     return <c>JsErrorInExceptionState</c>.
    ///     </para>
    ///     <para>
    ///     Requires an active script context.
    ///     </para>
    /// </remarks>
    /// <param name="exception">
    ///     The JavaScript exception to set for the runtime of the current context.
    /// </param>
    /// <returns>
    ///     JsNoError if the engine was set into an exception state, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsSetException(
            _In_ JsValueRef exception);

    /// <summary>
    ///     Suspends script execution and terminates any running scripts in a runtime.
    /// </summary>
    /// <remarks>
    ///     <para>
    ///     Calls to a suspended runtime will fail until <c>JsEnableRuntimeExecution</c> is called.
    ///     </para>
    ///     <para>
    ///     This API does not have to be called on the thread the runtime is active on. Although the
    ///     runtime will be set into a suspended state, an executing script may not be suspended
    ///     immediately; a running script will be terminated with an uncatchable exception as soon as
    ///     possible.
    ///     </para>
    ///     <para>
    ///     Suspending execution in a runtime that is already suspended is a no-op.
    ///     </para>
    /// </remarks>
    /// <param name="runtime">The runtime to be suspended.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsDisableRuntimeExecution(
            _In_ JsRuntimeHandle runtime);

    /// <summary>
    ///     Enables script execution in a runtime.
    /// </summary>
    /// <remarks>
    ///     Enabling script execution in a runtime that already has script execution enabled is a
    ///     no-op.
    /// </remarks>
    /// <param name="runtime">The runtime to be enabled.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsEnableRuntimeExecution(
            _In_ JsRuntimeHandle runtime);

    /// <summary>
    ///     Returns a value that indicates whether script execution is disabled in the runtime.
    /// </summary>
    /// <param name="runtime">Specifies the runtime to check if execution is disabled.</param>
    /// <param name="isDisabled">If execution is disabled, <c>true</c>, <c>false</c> otherwise.</param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsIsRuntimeExecutionDisabled(
            _In_ JsRuntimeHandle runtime,
            _Out_ bool *isDisabled);


    /// <summary>
    ///     A promise continuation callback.
    /// </summary>
    /// <remarks>
    ///     The host can specify a promise continuation callback in <c>JsSetPromiseContinuationCallback</c>. If
    ///     a script creates a task to be run later, then the promise continuation callback will be called with
    ///     the task and the task should be put in a FIFO queue, to be run when the current script is
    ///     done executing.
    /// </remarks>
    /// <param name="task">The task, represented as a JavaScript function.</param>
    /// <param name="callbackState">The data argument to be passed to the callback.</param>
    typedef void (CALLBACK *JsPromiseContinuationCallback)(_In_ JsValueRef task, _In_opt_ void *callbackState);

    /// <summary>
    ///     Sets a promise continuation callback function that is called by the context when a task
    ///     needs to be queued for future execution
    /// </summary>
    /// <remarks>
    ///     <para>
    ///     Requires an active script context.
    ///     </para>
    /// </remarks>
    /// <param name="promiseContinuationCallback">The callback function being set.</param>
    /// <param name="callbackState">
    ///     User provided state that will be passed back to the callback.
    /// </param>
    /// <returns>
    ///     The code <c>JsNoError</c> if the operation succeeded, a failure code otherwise.
    /// </returns>
    STDAPI_(JsErrorCode)
        JsSetPromiseContinuationCallback(
            _In_ JsPromiseContinuationCallback promiseContinuationCallback,
            _In_opt_ void *callbackState);
#endif // _CHAKRACOMMON_H_
