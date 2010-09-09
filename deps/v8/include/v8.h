// Copyright 2007-2009 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

/** \mainpage V8 API Reference Guide
 *
 * V8 is Google's open source JavaScript engine.
 *
 * This set of documents provides reference material generated from the
 * V8 header file, include/v8.h.
 *
 * For other documentation see http://code.google.com/apis/v8/
 */

#ifndef V8_H_
#define V8_H_

#include <stdio.h>

#ifdef _WIN32
// When compiling on MinGW stdint.h is available.
#ifdef __MINGW32__
#include <stdint.h>
#else  // __MINGW32__
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;  // NOLINT
typedef unsigned short uint16_t;  // NOLINT
typedef int int32_t;
typedef unsigned int uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
// intptr_t and friends are defined in crtdefs.h through stdio.h.
#endif  // __MINGW32__

// Setup for Windows DLL export/import. When building the V8 DLL the
// BUILDING_V8_SHARED needs to be defined. When building a program which uses
// the V8 DLL USING_V8_SHARED needs to be defined. When either building the V8
// static library or building a program which uses the V8 static library neither
// BUILDING_V8_SHARED nor USING_V8_SHARED should be defined.
#if defined(BUILDING_V8_SHARED) && defined(USING_V8_SHARED)
#error both BUILDING_V8_SHARED and USING_V8_SHARED are set - please check the\
  build configuration to ensure that at most one of these is set
#endif

#ifdef BUILDING_V8_SHARED
#define V8EXPORT __declspec(dllexport)
#elif USING_V8_SHARED
#define V8EXPORT __declspec(dllimport)
#else
#define V8EXPORT
#endif  // BUILDING_V8_SHARED

#else  // _WIN32

#include <stdint.h>

// Setup for Linux shared library export. There is no need to distinguish
// between building or using the V8 shared library, but we should not
// export symbols when we are building a static library.
#if defined(__GNUC__) && (__GNUC__ >= 4) && defined(V8_SHARED)
#define V8EXPORT __attribute__ ((visibility("default")))
#else  // defined(__GNUC__) && (__GNUC__ >= 4)
#define V8EXPORT
#endif  // defined(__GNUC__) && (__GNUC__ >= 4)

#endif  // _WIN32

/**
 * The v8 JavaScript engine.
 */
namespace v8 {

class Context;
class String;
class Value;
class Utils;
class Number;
class Object;
class Array;
class Int32;
class Uint32;
class External;
class Primitive;
class Boolean;
class Integer;
class Function;
class Date;
class ImplementationUtilities;
class Signature;
template <class T> class Handle;
template <class T> class Local;
template <class T> class Persistent;
class FunctionTemplate;
class ObjectTemplate;
class Data;
class AccessorInfo;
class StackTrace;
class StackFrame;

namespace internal {

class Arguments;
class Object;
class Heap;
class Top;

}


// --- W e a k  H a n d l e s


/**
 * A weak reference callback function.
 *
 * This callback should either explicitly invoke Dispose on |object| if
 * V8 wrapper is not needed anymore, or 'revive' it by invocation of MakeWeak.
 *
 * \param object the weak global object to be reclaimed by the garbage collector
 * \param parameter the value passed in when making the weak global object
 */
typedef void (*WeakReferenceCallback)(Persistent<Value> object,
                                      void* parameter);


// --- H a n d l e s ---

#define TYPE_CHECK(T, S)                                       \
  while (false) {                                              \
    *(static_cast<T* volatile*>(0)) = static_cast<S*>(0);      \
  }

/**
 * An object reference managed by the v8 garbage collector.
 *
 * All objects returned from v8 have to be tracked by the garbage
 * collector so that it knows that the objects are still alive.  Also,
 * because the garbage collector may move objects, it is unsafe to
 * point directly to an object.  Instead, all objects are stored in
 * handles which are known by the garbage collector and updated
 * whenever an object moves.  Handles should always be passed by value
 * (except in cases like out-parameters) and they should never be
 * allocated on the heap.
 *
 * There are two types of handles: local and persistent handles.
 * Local handles are light-weight and transient and typically used in
 * local operations.  They are managed by HandleScopes.  Persistent
 * handles can be used when storing objects across several independent
 * operations and have to be explicitly deallocated when they're no
 * longer used.
 *
 * It is safe to extract the object stored in the handle by
 * dereferencing the handle (for instance, to extract the Object* from
 * an Handle<Object>); the value will still be governed by a handle
 * behind the scenes and the same rules apply to these values as to
 * their handles.
 */
template <class T> class Handle {
 public:

  /**
   * Creates an empty handle.
   */
  inline Handle();

  /**
   * Creates a new handle for the specified value.
   */
  inline explicit Handle(T* val) : val_(val) { }

  /**
   * Creates a handle for the contents of the specified handle.  This
   * constructor allows you to pass handles as arguments by value and
   * to assign between handles.  However, if you try to assign between
   * incompatible handles, for instance from a Handle<String> to a
   * Handle<Number> it will cause a compiletime error.  Assigning
   * between compatible handles, for instance assigning a
   * Handle<String> to a variable declared as Handle<Value>, is legal
   * because String is a subclass of Value.
   */
  template <class S> inline Handle(Handle<S> that)
      : val_(reinterpret_cast<T*>(*that)) {
    /**
     * This check fails when trying to convert between incompatible
     * handles. For example, converting from a Handle<String> to a
     * Handle<Number>.
     */
    TYPE_CHECK(T, S);
  }

  /**
   * Returns true if the handle is empty.
   */
  inline bool IsEmpty() const { return val_ == 0; }

  inline T* operator->() const { return val_; }

  inline T* operator*() const { return val_; }

  /**
   * Sets the handle to be empty. IsEmpty() will then return true.
   */
  inline void Clear() { this->val_ = 0; }

  /**
   * Checks whether two handles are the same.
   * Returns true if both are empty, or if the objects
   * to which they refer are identical.
   * The handles' references are not checked.
   */
  template <class S> inline bool operator==(Handle<S> that) const {
    internal::Object** a = reinterpret_cast<internal::Object**>(**this);
    internal::Object** b = reinterpret_cast<internal::Object**>(*that);
    if (a == 0) return b == 0;
    if (b == 0) return false;
    return *a == *b;
  }

  /**
   * Checks whether two handles are different.
   * Returns true if only one of the handles is empty, or if
   * the objects to which they refer are different.
   * The handles' references are not checked.
   */
  template <class S> inline bool operator!=(Handle<S> that) const {
    return !operator==(that);
  }

  template <class S> static inline Handle<T> Cast(Handle<S> that) {
#ifdef V8_ENABLE_CHECKS
    // If we're going to perform the type check then we have to check
    // that the handle isn't empty before doing the checked cast.
    if (that.IsEmpty()) return Handle<T>();
#endif
    return Handle<T>(T::Cast(*that));
  }

  template <class S> inline Handle<S> As() {
    return Handle<S>::Cast(*this);
  }

 private:
  T* val_;
};


/**
 * A light-weight stack-allocated object handle.  All operations
 * that return objects from within v8 return them in local handles.  They
 * are created within HandleScopes, and all local handles allocated within a
 * handle scope are destroyed when the handle scope is destroyed.  Hence it
 * is not necessary to explicitly deallocate local handles.
 */
template <class T> class Local : public Handle<T> {
 public:
  inline Local();
  template <class S> inline Local(Local<S> that)
      : Handle<T>(reinterpret_cast<T*>(*that)) {
    /**
     * This check fails when trying to convert between incompatible
     * handles. For example, converting from a Handle<String> to a
     * Handle<Number>.
     */
    TYPE_CHECK(T, S);
  }
  template <class S> inline Local(S* that) : Handle<T>(that) { }
  template <class S> static inline Local<T> Cast(Local<S> that) {
#ifdef V8_ENABLE_CHECKS
    // If we're going to perform the type check then we have to check
    // that the handle isn't empty before doing the checked cast.
    if (that.IsEmpty()) return Local<T>();
#endif
    return Local<T>(T::Cast(*that));
  }

  template <class S> inline Local<S> As() {
    return Local<S>::Cast(*this);
  }

  /** Create a local handle for the content of another handle.
   *  The referee is kept alive by the local handle even when
   *  the original handle is destroyed/disposed.
   */
  inline static Local<T> New(Handle<T> that);
};


/**
 * An object reference that is independent of any handle scope.  Where
 * a Local handle only lives as long as the HandleScope in which it was
 * allocated, a Persistent handle remains valid until it is explicitly
 * disposed.
 *
 * A persistent handle contains a reference to a storage cell within
 * the v8 engine which holds an object value and which is updated by
 * the garbage collector whenever the object is moved.  A new storage
 * cell can be created using Persistent::New and existing handles can
 * be disposed using Persistent::Dispose.  Since persistent handles
 * are passed by value you may have many persistent handle objects
 * that point to the same storage cell.  For instance, if you pass a
 * persistent handle as an argument to a function you will not get two
 * different storage cells but rather two references to the same
 * storage cell.
 */
template <class T> class Persistent : public Handle<T> {
 public:

  /**
   * Creates an empty persistent handle that doesn't point to any
   * storage cell.
   */
  inline Persistent();

  /**
   * Creates a persistent handle for the same storage cell as the
   * specified handle.  This constructor allows you to pass persistent
   * handles as arguments by value and to assign between persistent
   * handles.  However, attempting to assign between incompatible
   * persistent handles, for instance from a Persistent<String> to a
   * Persistent<Number> will cause a compiletime error.  Assigning
   * between compatible persistent handles, for instance assigning a
   * Persistent<String> to a variable declared as Persistent<Value>,
   * is allowed as String is a subclass of Value.
   */
  template <class S> inline Persistent(Persistent<S> that)
      : Handle<T>(reinterpret_cast<T*>(*that)) {
    /**
     * This check fails when trying to convert between incompatible
     * handles. For example, converting from a Handle<String> to a
     * Handle<Number>.
     */
    TYPE_CHECK(T, S);
  }

  template <class S> inline Persistent(S* that) : Handle<T>(that) { }

  /**
   * "Casts" a plain handle which is known to be a persistent handle
   * to a persistent handle.
   */
  template <class S> explicit inline Persistent(Handle<S> that)
      : Handle<T>(*that) { }

  template <class S> static inline Persistent<T> Cast(Persistent<S> that) {
#ifdef V8_ENABLE_CHECKS
    // If we're going to perform the type check then we have to check
    // that the handle isn't empty before doing the checked cast.
    if (that.IsEmpty()) return Persistent<T>();
#endif
    return Persistent<T>(T::Cast(*that));
  }

  template <class S> inline Persistent<S> As() {
    return Persistent<S>::Cast(*this);
  }

  /**
   * Creates a new persistent handle for an existing local or
   * persistent handle.
   */
  inline static Persistent<T> New(Handle<T> that);

  /**
   * Releases the storage cell referenced by this persistent handle.
   * Does not remove the reference to the cell from any handles.
   * This handle's reference, and any any other references to the storage
   * cell remain and IsEmpty will still return false.
   */
  inline void Dispose();

  /**
   * Make the reference to this object weak.  When only weak handles
   * refer to the object, the garbage collector will perform a
   * callback to the given V8::WeakReferenceCallback function, passing
   * it the object reference and the given parameters.
   */
  inline void MakeWeak(void* parameters, WeakReferenceCallback callback);

  /** Clears the weak reference to this object.*/
  inline void ClearWeak();

  /**
   *Checks if the handle holds the only reference to an object.
   */
  inline bool IsNearDeath() const;

  /**
   * Returns true if the handle's reference is weak.
   */
  inline bool IsWeak() const;

 private:
  friend class ImplementationUtilities;
  friend class ObjectTemplate;
};


 /**
 * A stack-allocated class that governs a number of local handles.
 * After a handle scope has been created, all local handles will be
 * allocated within that handle scope until either the handle scope is
 * deleted or another handle scope is created.  If there is already a
 * handle scope and a new one is created, all allocations will take
 * place in the new handle scope until it is deleted.  After that,
 * new handles will again be allocated in the original handle scope.
 *
 * After the handle scope of a local handle has been deleted the
 * garbage collector will no longer track the object stored in the
 * handle and may deallocate it.  The behavior of accessing a handle
 * for which the handle scope has been deleted is undefined.
 */
class V8EXPORT HandleScope {
 public:
  HandleScope();

  ~HandleScope();

  /**
   * Closes the handle scope and returns the value as a handle in the
   * previous scope, which is the new current scope after the call.
   */
  template <class T> Local<T> Close(Handle<T> value);

  /**
   * Counts the number of allocated handles.
   */
  static int NumberOfHandles();

  /**
   * Creates a new handle with the given value.
   */
  static internal::Object** CreateHandle(internal::Object* value);

 private:
  // Make it impossible to create heap-allocated or illegal handle
  // scopes by disallowing certain operations.
  HandleScope(const HandleScope&);
  void operator=(const HandleScope&);
  void* operator new(size_t size);
  void operator delete(void*, size_t);

  // This Data class is accessible internally as HandleScopeData through a
  // typedef in the ImplementationUtilities class.
  class V8EXPORT Data {
   public:
    int extensions;
    internal::Object** next;
    internal::Object** limit;
    inline void Initialize() {
      extensions = -1;
      next = limit = NULL;
    }
  };

  Data previous_;

  // Allow for the active closing of HandleScopes which allows to pass a handle
  // from the HandleScope being closed to the next top most HandleScope.
  bool is_closed_;
  internal::Object** RawClose(internal::Object** value);

  friend class ImplementationUtilities;
};


// --- S p e c i a l   o b j e c t s ---


/**
 * The superclass of values and API object templates.
 */
class V8EXPORT Data {
 private:
  Data();
};


/**
 * Pre-compilation data that can be associated with a script.  This
 * data can be calculated for a script in advance of actually
 * compiling it, and can be stored between compilations.  When script
 * data is given to the compile method compilation will be faster.
 */
class V8EXPORT ScriptData {  // NOLINT
 public:
  virtual ~ScriptData() { }

  /**
   * Pre-compiles the specified script (context-independent).
   *
   * \param input Pointer to UTF-8 script source code.
   * \param length Length of UTF-8 script source code.
   */
  static ScriptData* PreCompile(const char* input, int length);

  /**
   * Pre-compiles the specified script (context-independent).
   *
   * NOTE: Pre-compilation using this method cannot happen on another thread
   * without using Lockers.
   *
   * \param source Script source code.
   */
  static ScriptData* PreCompile(Handle<String> source);

  /**
   * Load previous pre-compilation data.
   *
   * \param data Pointer to data returned by a call to Data() of a previous
   *   ScriptData. Ownership is not transferred.
   * \param length Length of data.
   */
  static ScriptData* New(const char* data, int length);

  /**
   * Returns the length of Data().
   */
  virtual int Length() = 0;

  /**
   * Returns a serialized representation of this ScriptData that can later be
   * passed to New(). NOTE: Serialized data is platform-dependent.
   */
  virtual const char* Data() = 0;

  /**
   * Returns true if the source code could not be parsed.
   */
  virtual bool HasError() = 0;
};


/**
 * The origin, within a file, of a script.
 */
class ScriptOrigin {
 public:
  inline ScriptOrigin(
      Handle<Value> resource_name,
      Handle<Integer> resource_line_offset = Handle<Integer>(),
      Handle<Integer> resource_column_offset = Handle<Integer>())
      : resource_name_(resource_name),
        resource_line_offset_(resource_line_offset),
        resource_column_offset_(resource_column_offset) { }
  inline Handle<Value> ResourceName() const;
  inline Handle<Integer> ResourceLineOffset() const;
  inline Handle<Integer> ResourceColumnOffset() const;
 private:
  Handle<Value> resource_name_;
  Handle<Integer> resource_line_offset_;
  Handle<Integer> resource_column_offset_;
};


/**
 * A compiled JavaScript script.
 */
class V8EXPORT Script {
 public:

  /**
   * Compiles the specified script (context-independent).
   *
   * \param source Script source code.
   * \param origin Script origin, owned by caller, no references are kept
   *   when New() returns
   * \param pre_data Pre-parsing data, as obtained by ScriptData::PreCompile()
   *   using pre_data speeds compilation if it's done multiple times.
   *   Owned by caller, no references are kept when New() returns.
   * \param script_data Arbitrary data associated with script. Using
   *   this has same effect as calling SetData(), but allows data to be
   *   available to compile event handlers.
   * \return Compiled script object (context independent; when run it
   *   will use the currently entered context).
   */
  static Local<Script> New(Handle<String> source,
                           ScriptOrigin* origin = NULL,
                           ScriptData* pre_data = NULL,
                           Handle<String> script_data = Handle<String>());

  /**
   * Compiles the specified script using the specified file name
   * object (typically a string) as the script's origin.
   *
   * \param source Script source code.
   * \param file_name file name object (typically a string) to be used
   *   as the script's origin.
   * \return Compiled script object (context independent; when run it
   *   will use the currently entered context).
   */
  static Local<Script> New(Handle<String> source,
                           Handle<Value> file_name);

  /**
   * Compiles the specified script (bound to current context).
   *
   * \param source Script source code.
   * \param origin Script origin, owned by caller, no references are kept
   *   when Compile() returns
   * \param pre_data Pre-parsing data, as obtained by ScriptData::PreCompile()
   *   using pre_data speeds compilation if it's done multiple times.
   *   Owned by caller, no references are kept when Compile() returns.
   * \param script_data Arbitrary data associated with script. Using
   *   this has same effect as calling SetData(), but makes data available
   *   earlier (i.e. to compile event handlers).
   * \return Compiled script object, bound to the context that was active
   *   when this function was called.  When run it will always use this
   *   context.
   */
  static Local<Script> Compile(Handle<String> source,
                               ScriptOrigin* origin = NULL,
                               ScriptData* pre_data = NULL,
                               Handle<String> script_data = Handle<String>());

  /**
   * Compiles the specified script using the specified file name
   * object (typically a string) as the script's origin.
   *
   * \param source Script source code.
   * \param file_name File name to use as script's origin
   * \param script_data Arbitrary data associated with script. Using
   *   this has same effect as calling SetData(), but makes data available
   *   earlier (i.e. to compile event handlers).
   * \return Compiled script object, bound to the context that was active
   *   when this function was called.  When run it will always use this
   *   context.
   */
  static Local<Script> Compile(Handle<String> source,
                               Handle<Value> file_name,
                               Handle<String> script_data = Handle<String>());

  /**
   * Runs the script returning the resulting value.  If the script is
   * context independent (created using ::New) it will be run in the
   * currently entered context.  If it is context specific (created
   * using ::Compile) it will be run in the context in which it was
   * compiled.
   */
  Local<Value> Run();

  /**
   * Returns the script id value.
   */
  Local<Value> Id();

  /**
   * Associate an additional data object with the script. This is mainly used
   * with the debugger as this data object is only available through the
   * debugger API.
   */
  void SetData(Handle<String> data);
};


/**
 * An error message.
 */
class V8EXPORT Message {
 public:
  Local<String> Get() const;
  Local<String> GetSourceLine() const;

  /**
   * Returns the resource name for the script from where the function causing
   * the error originates.
   */
  Handle<Value> GetScriptResourceName() const;

  /**
   * Returns the resource data for the script from where the function causing
   * the error originates.
   */
  Handle<Value> GetScriptData() const;

  /**
   * Exception stack trace. By default stack traces are not captured for
   * uncaught exceptions. SetCaptureStackTraceForUncaughtExceptions allows
   * to change this option.
   */
  Handle<StackTrace> GetStackTrace() const;

  /**
   * Returns the number, 1-based, of the line where the error occurred.
   */
  int GetLineNumber() const;

  /**
   * Returns the index within the script of the first character where
   * the error occurred.
   */
  int GetStartPosition() const;

  /**
   * Returns the index within the script of the last character where
   * the error occurred.
   */
  int GetEndPosition() const;

  /**
   * Returns the index within the line of the first character where
   * the error occurred.
   */
  int GetStartColumn() const;

  /**
   * Returns the index within the line of the last character where
   * the error occurred.
   */
  int GetEndColumn() const;

  // TODO(1245381): Print to a string instead of on a FILE.
  static void PrintCurrentStackTrace(FILE* out);

  static const int kNoLineNumberInfo = 0;
  static const int kNoColumnInfo = 0;
};


/**
 * Representation of a JavaScript stack trace. The information collected is a
 * snapshot of the execution stack and the information remains valid after
 * execution continues.
 */
class V8EXPORT StackTrace {
 public:
  /**
   * Flags that determine what information is placed captured for each
   * StackFrame when grabbing the current stack trace.
   */
  enum StackTraceOptions {
    kLineNumber = 1,
    kColumnOffset = 1 << 1 | kLineNumber,
    kScriptName = 1 << 2,
    kFunctionName = 1 << 3,
    kIsEval = 1 << 4,
    kIsConstructor = 1 << 5,
    kOverview = kLineNumber | kColumnOffset | kScriptName | kFunctionName,
    kDetailed = kOverview | kIsEval | kIsConstructor
  };

  /**
   * Returns a StackFrame at a particular index.
   */
  Local<StackFrame> GetFrame(uint32_t index) const;

  /**
   * Returns the number of StackFrames.
   */
  int GetFrameCount() const;

  /**
   * Returns StackTrace as a v8::Array that contains StackFrame objects.
   */
  Local<Array> AsArray();

  /**
   * Grab a snapshot of the the current JavaScript execution stack.
   *
   * \param frame_limit The maximum number of stack frames we want to capture.
   * \param options Enumerates the set of things we will capture for each
   *   StackFrame.
   */
  static Local<StackTrace> CurrentStackTrace(
      int frame_limit,
      StackTraceOptions options = kOverview);
};


/**
 * A single JavaScript stack frame.
 */
class V8EXPORT StackFrame {
 public:
  /**
   * Returns the number, 1-based, of the line for the associate function call.
   * This method will return Message::kNoLineNumberInfo if it is unable to
   * retrieve the line number, or if kLineNumber was not passed as an option
   * when capturing the StackTrace.
   */
  int GetLineNumber() const;

  /**
   * Returns the 1-based column offset on the line for the associated function
   * call.
   * This method will return Message::kNoColumnInfo if it is unable to retrieve
   * the column number, or if kColumnOffset was not passed as an option when
   * capturing the StackTrace.
   */
  int GetColumn() const;

  /**
   * Returns the name of the resource that contains the script for the
   * function for this StackFrame.
   */
  Local<String> GetScriptName() const;

  /**
   * Returns the name of the function associated with this stack frame.
   */
  Local<String> GetFunctionName() const;

  /**
   * Returns whether or not the associated function is compiled via a call to
   * eval().
   */
  bool IsEval() const;

  /**
   * Returns whther or not the associated function is called as a
   * constructor via "new".
   */
  bool IsConstructor() const;
};


// --- V a l u e ---


/**
 * The superclass of all JavaScript values and objects.
 */
class Value : public Data {
 public:

  /**
   * Returns true if this value is the undefined value.  See ECMA-262
   * 4.3.10.
   */
  V8EXPORT bool IsUndefined() const;

  /**
   * Returns true if this value is the null value.  See ECMA-262
   * 4.3.11.
   */
  V8EXPORT bool IsNull() const;

   /**
   * Returns true if this value is true.
   */
  V8EXPORT bool IsTrue() const;

  /**
   * Returns true if this value is false.
   */
  V8EXPORT bool IsFalse() const;

  /**
   * Returns true if this value is an instance of the String type.
   * See ECMA-262 8.4.
   */
  inline bool IsString() const;

  /**
   * Returns true if this value is a function.
   */
  V8EXPORT bool IsFunction() const;

  /**
   * Returns true if this value is an array.
   */
  V8EXPORT bool IsArray() const;

  /**
   * Returns true if this value is an object.
   */
  V8EXPORT bool IsObject() const;

  /**
   * Returns true if this value is boolean.
   */
  V8EXPORT bool IsBoolean() const;

  /**
   * Returns true if this value is a number.
   */
  V8EXPORT bool IsNumber() const;

  /**
   * Returns true if this value is external.
   */
  V8EXPORT bool IsExternal() const;

  /**
   * Returns true if this value is a 32-bit signed integer.
   */
  V8EXPORT bool IsInt32() const;

  /**
   * Returns true if this value is a 32-bit unsigned integer.
   */
  V8EXPORT bool IsUint32() const;

  /**
   * Returns true if this value is a Date.
   */
  V8EXPORT bool IsDate() const;

  /**
   * Returns true if this value is a RegExp.
   */
  V8EXPORT bool IsRegExp() const;

  V8EXPORT Local<Boolean> ToBoolean() const;
  V8EXPORT Local<Number> ToNumber() const;
  V8EXPORT Local<String> ToString() const;
  V8EXPORT Local<String> ToDetailString() const;
  V8EXPORT Local<Object> ToObject() const;
  V8EXPORT Local<Integer> ToInteger() const;
  V8EXPORT Local<Uint32> ToUint32() const;
  V8EXPORT Local<Int32> ToInt32() const;

  /**
   * Attempts to convert a string to an array index.
   * Returns an empty handle if the conversion fails.
   */
  V8EXPORT Local<Uint32> ToArrayIndex() const;

  V8EXPORT bool BooleanValue() const;
  V8EXPORT double NumberValue() const;
  V8EXPORT int64_t IntegerValue() const;
  V8EXPORT uint32_t Uint32Value() const;
  V8EXPORT int32_t Int32Value() const;

  /** JS == */
  V8EXPORT bool Equals(Handle<Value> that) const;
  V8EXPORT bool StrictEquals(Handle<Value> that) const;

 private:
  inline bool QuickIsString() const;
  V8EXPORT bool FullIsString() const;
};


/**
 * The superclass of primitive values.  See ECMA-262 4.3.2.
 */
class Primitive : public Value { };


/**
 * A primitive boolean value (ECMA-262, 4.3.14).  Either the true
 * or false value.
 */
class Boolean : public Primitive {
 public:
  V8EXPORT bool Value() const;
  static inline Handle<Boolean> New(bool value);
};


/**
 * A JavaScript string value (ECMA-262, 4.3.17).
 */
class String : public Primitive {
 public:

  /**
   * Returns the number of characters in this string.
   */
  V8EXPORT int Length() const;

  /**
   * Returns the number of bytes in the UTF-8 encoded
   * representation of this string.
   */
  V8EXPORT int Utf8Length() const;

  /**
   * Write the contents of the string to an external buffer.
   * If no arguments are given, expects the buffer to be large
   * enough to hold the entire string and NULL terminator. Copies
   * the contents of the string and the NULL terminator into the
   * buffer.
   *
   * Copies up to length characters into the output buffer.
   * Only null-terminates if there is enough space in the buffer.
   *
   * \param buffer The buffer into which the string will be copied.
   * \param start The starting position within the string at which
   * copying begins.
   * \param length The number of bytes to copy from the string.
   * \param nchars_ref The number of characters written, can be NULL.
   * \param hints Various hints that might affect performance of this or
   *    subsequent operations.
   * \return The number of bytes copied to the buffer
   * excluding the NULL terminator.
   */
  enum WriteHints {
    NO_HINTS = 0,
    HINT_MANY_WRITES_EXPECTED = 1
  };

  V8EXPORT int Write(uint16_t* buffer,
                     int start = 0,
                     int length = -1,
                     WriteHints hints = NO_HINTS) const;  // UTF-16
  V8EXPORT int WriteAscii(char* buffer,
                          int start = 0,
                          int length = -1,
                          WriteHints hints = NO_HINTS) const;  // ASCII
  V8EXPORT int WriteUtf8(char* buffer,
                         int length = -1,
                         int* nchars_ref = NULL,
                         WriteHints hints = NO_HINTS) const;  // UTF-8

  /**
   * A zero length string.
   */
  V8EXPORT static v8::Local<v8::String> Empty();

  /**
   * Returns true if the string is external
   */
  V8EXPORT bool IsExternal() const;

  /**
   * Returns true if the string is both external and ascii
   */
  V8EXPORT bool IsExternalAscii() const;

  class V8EXPORT ExternalStringResourceBase {
   public:
    virtual ~ExternalStringResourceBase() {}

   protected:
    ExternalStringResourceBase() {}

    /**
     * Internally V8 will call this Dispose method when the external string
     * resource is no longer needed. The default implementation will use the
     * delete operator. This method can be overridden in subclasses to
     * control how allocated external string resources are disposed.
     */
    virtual void Dispose() { delete this; }

   private:
    // Disallow copying and assigning.
    ExternalStringResourceBase(const ExternalStringResourceBase&);
    void operator=(const ExternalStringResourceBase&);

    friend class v8::internal::Heap;
  };

  /**
   * An ExternalStringResource is a wrapper around a two-byte string
   * buffer that resides outside V8's heap. Implement an
   * ExternalStringResource to manage the life cycle of the underlying
   * buffer.  Note that the string data must be immutable.
   */
  class V8EXPORT ExternalStringResource
      : public ExternalStringResourceBase {
   public:
    /**
     * Override the destructor to manage the life cycle of the underlying
     * buffer.
     */
    virtual ~ExternalStringResource() {}

    /**
     * The string data from the underlying buffer.
     */
    virtual const uint16_t* data() const = 0;

    /**
     * The length of the string. That is, the number of two-byte characters.
     */
    virtual size_t length() const = 0;

   protected:
    ExternalStringResource() {}
  };

  /**
   * An ExternalAsciiStringResource is a wrapper around an ascii
   * string buffer that resides outside V8's heap. Implement an
   * ExternalAsciiStringResource to manage the life cycle of the
   * underlying buffer.  Note that the string data must be immutable
   * and that the data must be strict 7-bit ASCII, not Latin1 or
   * UTF-8, which would require special treatment internally in the
   * engine and, in the case of UTF-8, do not allow efficient indexing.
   * Use String::New or convert to 16 bit data for non-ASCII.
   */

  class V8EXPORT ExternalAsciiStringResource
      : public ExternalStringResourceBase {
   public:
    /**
     * Override the destructor to manage the life cycle of the underlying
     * buffer.
     */
    virtual ~ExternalAsciiStringResource() {}
    /** The string data from the underlying buffer.*/
    virtual const char* data() const = 0;
    /** The number of ascii characters in the string.*/
    virtual size_t length() const = 0;
   protected:
    ExternalAsciiStringResource() {}
  };

  /**
   * Get the ExternalStringResource for an external string.  Returns
   * NULL if IsExternal() doesn't return true.
   */
  inline ExternalStringResource* GetExternalStringResource() const;

  /**
   * Get the ExternalAsciiStringResource for an external ascii string.
   * Returns NULL if IsExternalAscii() doesn't return true.
   */
  V8EXPORT ExternalAsciiStringResource* GetExternalAsciiStringResource() const;

  static inline String* Cast(v8::Value* obj);

  /**
   * Allocates a new string from either utf-8 encoded or ascii data.
   * The second parameter 'length' gives the buffer length.
   * If the data is utf-8 encoded, the caller must
   * be careful to supply the length parameter.
   * If it is not given, the function calls
   * 'strlen' to determine the buffer length, it might be
   * wrong if 'data' contains a null character.
   */
  V8EXPORT static Local<String> New(const char* data, int length = -1);

  /** Allocates a new string from utf16 data.*/
  V8EXPORT static Local<String> New(const uint16_t* data, int length = -1);

  /** Creates a symbol. Returns one if it exists already.*/
  V8EXPORT static Local<String> NewSymbol(const char* data, int length = -1);

  /**
   * Creates a new string by concatenating the left and the right strings
   * passed in as parameters.
   */
  V8EXPORT static Local<String> Concat(Handle<String> left,
                                       Handle<String>right);

  /**
   * Creates a new external string using the data defined in the given
   * resource. When the external string is no longer live on V8's heap the
   * resource will be disposed by calling its Dispose method. The caller of
   * this function should not otherwise delete or modify the resource. Neither
   * should the underlying buffer be deallocated or modified except through the
   * destructor of the external string resource.
   */
  V8EXPORT static Local<String> NewExternal(ExternalStringResource* resource);

  /**
   * Associate an external string resource with this string by transforming it
   * in place so that existing references to this string in the JavaScript heap
   * will use the external string resource. The external string resource's
   * character contents needs to be equivalent to this string.
   * Returns true if the string has been changed to be an external string.
   * The string is not modified if the operation fails. See NewExternal for
   * information on the lifetime of the resource.
   */
  V8EXPORT bool MakeExternal(ExternalStringResource* resource);

  /**
   * Creates a new external string using the ascii data defined in the given
   * resource. When the external string is no longer live on V8's heap the
   * resource will be disposed by calling its Dispose method. The caller of
   * this function should not otherwise delete or modify the resource. Neither
   * should the underlying buffer be deallocated or modified except through the
   * destructor of the external string resource.
   */
  V8EXPORT static Local<String> NewExternal(
      ExternalAsciiStringResource* resource);

  /**
   * Associate an external string resource with this string by transforming it
   * in place so that existing references to this string in the JavaScript heap
   * will use the external string resource. The external string resource's
   * character contents needs to be equivalent to this string.
   * Returns true if the string has been changed to be an external string.
   * The string is not modified if the operation fails. See NewExternal for
   * information on the lifetime of the resource.
   */
  V8EXPORT bool MakeExternal(ExternalAsciiStringResource* resource);

  /**
   * Returns true if this string can be made external.
   */
  V8EXPORT bool CanMakeExternal();

  /** Creates an undetectable string from the supplied ascii or utf-8 data.*/
  V8EXPORT static Local<String> NewUndetectable(const char* data,
                                                int length = -1);

  /** Creates an undetectable string from the supplied utf-16 data.*/
  V8EXPORT static Local<String> NewUndetectable(const uint16_t* data,
                                                int length = -1);

  /**
   * Converts an object to a utf8-encoded character array.  Useful if
   * you want to print the object.  If conversion to a string fails
   * (eg. due to an exception in the toString() method of the object)
   * then the length() method returns 0 and the * operator returns
   * NULL.
   */
  class V8EXPORT Utf8Value {
   public:
    explicit Utf8Value(Handle<v8::Value> obj);
    ~Utf8Value();
    char* operator*() { return str_; }
    const char* operator*() const { return str_; }
    int length() const { return length_; }
   private:
    char* str_;
    int length_;

    // Disallow copying and assigning.
    Utf8Value(const Utf8Value&);
    void operator=(const Utf8Value&);
  };

  /**
   * Converts an object to an ascii string.
   * Useful if you want to print the object.
   * If conversion to a string fails (eg. due to an exception in the toString()
   * method of the object) then the length() method returns 0 and the * operator
   * returns NULL.
   */
  class V8EXPORT AsciiValue {
   public:
    explicit AsciiValue(Handle<v8::Value> obj);
    ~AsciiValue();
    char* operator*() { return str_; }
    const char* operator*() const { return str_; }
    int length() const { return length_; }
   private:
    char* str_;
    int length_;

    // Disallow copying and assigning.
    AsciiValue(const AsciiValue&);
    void operator=(const AsciiValue&);
  };

  /**
   * Converts an object to a two-byte string.
   * If conversion to a string fails (eg. due to an exception in the toString()
   * method of the object) then the length() method returns 0 and the * operator
   * returns NULL.
   */
  class V8EXPORT Value {
   public:
    explicit Value(Handle<v8::Value> obj);
    ~Value();
    uint16_t* operator*() { return str_; }
    const uint16_t* operator*() const { return str_; }
    int length() const { return length_; }
   private:
    uint16_t* str_;
    int length_;

    // Disallow copying and assigning.
    Value(const Value&);
    void operator=(const Value&);
  };

 private:
  V8EXPORT void VerifyExternalStringResource(ExternalStringResource* val) const;
  V8EXPORT static void CheckCast(v8::Value* obj);
};


/**
 * A JavaScript number value (ECMA-262, 4.3.20)
 */
class Number : public Primitive {
 public:
  V8EXPORT double Value() const;
  V8EXPORT static Local<Number> New(double value);
  static inline Number* Cast(v8::Value* obj);
 private:
  V8EXPORT Number();
  static void CheckCast(v8::Value* obj);
};


/**
 * A JavaScript value representing a signed integer.
 */
class Integer : public Number {
 public:
  V8EXPORT static Local<Integer> New(int32_t value);
  V8EXPORT static Local<Integer> NewFromUnsigned(uint32_t value);
  V8EXPORT int64_t Value() const;
  static inline Integer* Cast(v8::Value* obj);
 private:
  V8EXPORT Integer();
  V8EXPORT static void CheckCast(v8::Value* obj);
};


/**
 * A JavaScript value representing a 32-bit signed integer.
 */
class Int32 : public Integer {
 public:
  V8EXPORT int32_t Value() const;
 private:
  V8EXPORT Int32();
};


/**
 * A JavaScript value representing a 32-bit unsigned integer.
 */
class Uint32 : public Integer {
 public:
  V8EXPORT uint32_t Value() const;
 private:
  V8EXPORT Uint32();
};


/**
 * An instance of the built-in Date constructor (ECMA-262, 15.9).
 */
class Date : public Value {
 public:
  V8EXPORT static Local<Value> New(double time);

  /**
   * A specialization of Value::NumberValue that is more efficient
   * because we know the structure of this object.
   */
  V8EXPORT double NumberValue() const;

  static inline Date* Cast(v8::Value* obj);
 private:
  V8EXPORT static void CheckCast(v8::Value* obj);
};


enum PropertyAttribute {
  None       = 0,
  ReadOnly   = 1 << 0,
  DontEnum   = 1 << 1,
  DontDelete = 1 << 2
};

enum ExternalArrayType {
  kExternalByteArray = 1,
  kExternalUnsignedByteArray,
  kExternalShortArray,
  kExternalUnsignedShortArray,
  kExternalIntArray,
  kExternalUnsignedIntArray,
  kExternalFloatArray
};

/**
 * Accessor[Getter|Setter] are used as callback functions when
 * setting|getting a particular property. See Object and ObjectTemplate's
 * method SetAccessor.
 */
typedef Handle<Value> (*AccessorGetter)(Local<String> property,
                                        const AccessorInfo& info);


typedef void (*AccessorSetter)(Local<String> property,
                               Local<Value> value,
                               const AccessorInfo& info);


/**
 * Access control specifications.
 *
 * Some accessors should be accessible across contexts.  These
 * accessors have an explicit access control parameter which specifies
 * the kind of cross-context access that should be allowed.
 *
 * Additionally, for security, accessors can prohibit overwriting by
 * accessors defined in JavaScript.  For objects that have such
 * accessors either locally or in their prototype chain it is not
 * possible to overwrite the accessor by using __defineGetter__ or
 * __defineSetter__ from JavaScript code.
 */
enum AccessControl {
  DEFAULT               = 0,
  ALL_CAN_READ          = 1,
  ALL_CAN_WRITE         = 1 << 1,
  PROHIBITS_OVERWRITING = 1 << 2
};


/**
 * A JavaScript object (ECMA-262, 4.3.3)
 */
class Object : public Value {
 public:
  V8EXPORT bool Set(Handle<Value> key,
                    Handle<Value> value,
                    PropertyAttribute attribs = None);

  V8EXPORT bool Set(uint32_t index,
                    Handle<Value> value);

  // Sets a local property on this object bypassing interceptors and
  // overriding accessors or read-only properties.
  //
  // Note that if the object has an interceptor the property will be set
  // locally, but since the interceptor takes precedence the local property
  // will only be returned if the interceptor doesn't return a value.
  //
  // Note also that this only works for named properties.
  V8EXPORT bool ForceSet(Handle<Value> key,
                         Handle<Value> value,
                         PropertyAttribute attribs = None);

  V8EXPORT Local<Value> Get(Handle<Value> key);

  V8EXPORT Local<Value> Get(uint32_t index);

  // TODO(1245389): Replace the type-specific versions of these
  // functions with generic ones that accept a Handle<Value> key.
  V8EXPORT bool Has(Handle<String> key);

  V8EXPORT bool Delete(Handle<String> key);

  // Delete a property on this object bypassing interceptors and
  // ignoring dont-delete attributes.
  V8EXPORT bool ForceDelete(Handle<Value> key);

  V8EXPORT bool Has(uint32_t index);

  V8EXPORT bool Delete(uint32_t index);

  V8EXPORT bool SetAccessor(Handle<String> name,
                            AccessorGetter getter,
                            AccessorSetter setter = 0,
                            Handle<Value> data = Handle<Value>(),
                            AccessControl settings = DEFAULT,
                            PropertyAttribute attribute = None);

  /**
   * Returns an array containing the names of the enumerable properties
   * of this object, including properties from prototype objects.  The
   * array returned by this method contains the same values as would
   * be enumerated by a for-in statement over this object.
   */
  V8EXPORT Local<Array> GetPropertyNames();

  /**
   * Get the prototype object.  This does not skip objects marked to
   * be skipped by __proto__ and it does not consult the security
   * handler.
   */
  V8EXPORT Local<Value> GetPrototype();

  /**
   * Set the prototype object.  This does not skip objects marked to
   * be skipped by __proto__ and it does not consult the security
   * handler.
   */
  V8EXPORT bool SetPrototype(Handle<Value> prototype);

  /**
   * Finds an instance of the given function template in the prototype
   * chain.
   */
  V8EXPORT Local<Object> FindInstanceInPrototypeChain(
      Handle<FunctionTemplate> tmpl);

  /**
   * Call builtin Object.prototype.toString on this object.
   * This is different from Value::ToString() that may call
   * user-defined toString function. This one does not.
   */
  V8EXPORT Local<String> ObjectProtoToString();

  /** Gets the number of internal fields for this Object. */
  V8EXPORT int InternalFieldCount();
  /** Gets the value in an internal field. */
  inline Local<Value> GetInternalField(int index);
  /** Sets the value in an internal field. */
  V8EXPORT void SetInternalField(int index, Handle<Value> value);

  /** Gets a native pointer from an internal field. */
  inline void* GetPointerFromInternalField(int index);

  /** Sets a native pointer in an internal field. */
  V8EXPORT void SetPointerInInternalField(int index, void* value);

  // Testers for local properties.
  V8EXPORT bool HasRealNamedProperty(Handle<String> key);
  V8EXPORT bool HasRealIndexedProperty(uint32_t index);
  V8EXPORT bool HasRealNamedCallbackProperty(Handle<String> key);

  /**
   * If result.IsEmpty() no real property was located in the prototype chain.
   * This means interceptors in the prototype chain are not called.
   */
  V8EXPORT Local<Value> GetRealNamedPropertyInPrototypeChain(
      Handle<String> key);

  /**
   * If result.IsEmpty() no real property was located on the object or
   * in the prototype chain.
   * This means interceptors in the prototype chain are not called.
   */
  V8EXPORT Local<Value> GetRealNamedProperty(Handle<String> key);

  /** Tests for a named lookup interceptor.*/
  V8EXPORT bool HasNamedLookupInterceptor();

  /** Tests for an index lookup interceptor.*/
  V8EXPORT bool HasIndexedLookupInterceptor();

  /**
   * Turns on access check on the object if the object is an instance of
   * a template that has access check callbacks. If an object has no
   * access check info, the object cannot be accessed by anyone.
   */
  V8EXPORT void TurnOnAccessCheck();

  /**
   * Returns the identity hash for this object. The current implemenation uses
   * a hidden property on the object to store the identity hash.
   *
   * The return value will never be 0. Also, it is not guaranteed to be
   * unique.
   */
  V8EXPORT int GetIdentityHash();

  /**
   * Access hidden properties on JavaScript objects. These properties are
   * hidden from the executing JavaScript and only accessible through the V8
   * C++ API. Hidden properties introduced by V8 internally (for example the
   * identity hash) are prefixed with "v8::".
   */
  V8EXPORT bool SetHiddenValue(Handle<String> key, Handle<Value> value);
  V8EXPORT Local<Value> GetHiddenValue(Handle<String> key);
  V8EXPORT bool DeleteHiddenValue(Handle<String> key);

  /**
   * Returns true if this is an instance of an api function (one
   * created from a function created from a function template) and has
   * been modified since it was created.  Note that this method is
   * conservative and may return true for objects that haven't actually
   * been modified.
   */
  V8EXPORT bool IsDirty();

  /**
   * Clone this object with a fast but shallow copy.  Values will point
   * to the same values as the original object.
   */
  V8EXPORT Local<Object> Clone();

  /**
   * Set the backing store of the indexed properties to be managed by the
   * embedding layer. Access to the indexed properties will follow the rules
   * spelled out in CanvasPixelArray.
   * Note: The embedding program still owns the data and needs to ensure that
   *       the backing store is preserved while V8 has a reference.
   */
  V8EXPORT void SetIndexedPropertiesToPixelData(uint8_t* data, int length);
  bool HasIndexedPropertiesInPixelData();
  uint8_t* GetIndexedPropertiesPixelData();
  int GetIndexedPropertiesPixelDataLength();

  /**
   * Set the backing store of the indexed properties to be managed by the
   * embedding layer. Access to the indexed properties will follow the rules
   * spelled out for the CanvasArray subtypes in the WebGL specification.
   * Note: The embedding program still owns the data and needs to ensure that
   *       the backing store is preserved while V8 has a reference.
   */
  V8EXPORT void SetIndexedPropertiesToExternalArrayData(
      void* data,
      ExternalArrayType array_type,
      int number_of_elements);
  bool HasIndexedPropertiesInExternalArrayData();
  void* GetIndexedPropertiesExternalArrayData();
  ExternalArrayType GetIndexedPropertiesExternalArrayDataType();
  int GetIndexedPropertiesExternalArrayDataLength();

  V8EXPORT static Local<Object> New();
  static inline Object* Cast(Value* obj);
 private:
  V8EXPORT Object();
  V8EXPORT static void CheckCast(Value* obj);
  V8EXPORT Local<Value> CheckedGetInternalField(int index);
  V8EXPORT void* SlowGetPointerFromInternalField(int index);

  /**
   * If quick access to the internal field is possible this method
   * returns the value.  Otherwise an empty handle is returned.
   */
  inline Local<Value> UncheckedGetInternalField(int index);
};


/**
 * An instance of the built-in array constructor (ECMA-262, 15.4.2).
 */
class Array : public Object {
 public:
  V8EXPORT uint32_t Length() const;

  /**
   * Clones an element at index |index|.  Returns an empty
   * handle if cloning fails (for any reason).
   */
  V8EXPORT Local<Object> CloneElementAt(uint32_t index);

  V8EXPORT static Local<Array> New(int length = 0);
  static inline Array* Cast(Value* obj);
 private:
  V8EXPORT Array();
  static void CheckCast(Value* obj);
};


/**
 * A JavaScript function object (ECMA-262, 15.3).
 */
class Function : public Object {
 public:
  V8EXPORT Local<Object> NewInstance() const;
  V8EXPORT Local<Object> NewInstance(int argc, Handle<Value> argv[]) const;
  V8EXPORT Local<Value> Call(Handle<Object> recv,
                             int argc,
                             Handle<Value> argv[]);
  V8EXPORT void SetName(Handle<String> name);
  V8EXPORT Handle<Value> GetName() const;

  /**
   * Returns zero based line number of function body and
   * kLineOffsetNotFound if no information available.
   */
  V8EXPORT int GetScriptLineNumber() const;
  V8EXPORT ScriptOrigin GetScriptOrigin() const;
  static inline Function* Cast(Value* obj);
  V8EXPORT static const int kLineOffsetNotFound;
 private:
  V8EXPORT Function();
  V8EXPORT static void CheckCast(Value* obj);
};


/**
 * A JavaScript value that wraps a C++ void*.  This type of value is
 * mainly used to associate C++ data structures with JavaScript
 * objects.
 *
 * The Wrap function V8 will return the most optimal Value object wrapping the
 * C++ void*. The type of the value is not guaranteed to be an External object
 * and no assumptions about its type should be made. To access the wrapped
 * value Unwrap should be used, all other operations on that object will lead
 * to unpredictable results.
 */
class External : public Value {
 public:
  V8EXPORT static Local<Value> Wrap(void* data);
  static inline void* Unwrap(Handle<Value> obj);

  V8EXPORT static Local<External> New(void* value);
  static inline External* Cast(Value* obj);
  V8EXPORT void* Value() const;
 private:
  V8EXPORT External();
  V8EXPORT static void CheckCast(v8::Value* obj);
  static inline void* QuickUnwrap(Handle<v8::Value> obj);
  V8EXPORT static void* FullUnwrap(Handle<v8::Value> obj);
};


// --- T e m p l a t e s ---


/**
 * The superclass of object and function templates.
 */
class V8EXPORT Template : public Data {
 public:
  /** Adds a property to each instance created by this template.*/
  void Set(Handle<String> name, Handle<Data> value,
           PropertyAttribute attributes = None);
  inline void Set(const char* name, Handle<Data> value);
 private:
  Template();

  friend class ObjectTemplate;
  friend class FunctionTemplate;
};


/**
 * The argument information given to function call callbacks.  This
 * class provides access to information about the context of the call,
 * including the receiver, the number and values of arguments, and
 * the holder of the function.
 */
class Arguments {
 public:
  inline int Length() const;
  inline Local<Value> operator[](int i) const;
  inline Local<Function> Callee() const;
  inline Local<Object> This() const;
  inline Local<Object> Holder() const;
  inline bool IsConstructCall() const;
  inline Local<Value> Data() const;
 private:
  friend class ImplementationUtilities;
  inline Arguments(Local<Value> data,
                   Local<Object> holder,
                   Local<Function> callee,
                   bool is_construct_call,
                   void** values, int length);
  Local<Value> data_;
  Local<Object> holder_;
  Local<Function> callee_;
  bool is_construct_call_;
  void** values_;
  int length_;
};


/**
 * The information passed to an accessor callback about the context
 * of the property access.
 */
class V8EXPORT AccessorInfo {
 public:
  inline AccessorInfo(internal::Object** args)
      : args_(args) { }
  inline Local<Value> Data() const;
  inline Local<Object> This() const;
  inline Local<Object> Holder() const;
 private:
  internal::Object** args_;
};


typedef Handle<Value> (*InvocationCallback)(const Arguments& args);

/**
 * NamedProperty[Getter|Setter] are used as interceptors on object.
 * See ObjectTemplate::SetNamedPropertyHandler.
 */
typedef Handle<Value> (*NamedPropertyGetter)(Local<String> property,
                                             const AccessorInfo& info);


/**
 * Returns the value if the setter intercepts the request.
 * Otherwise, returns an empty handle.
 */
typedef Handle<Value> (*NamedPropertySetter)(Local<String> property,
                                             Local<Value> value,
                                             const AccessorInfo& info);

/**
 * Returns a non-empty handle if the interceptor intercepts the request.
 * The result is an integer encoding property attributes (like v8::None,
 * v8::DontEnum, etc.)
 */
typedef Handle<Integer> (*NamedPropertyQuery)(Local<String> property,
                                              const AccessorInfo& info);


/**
 * Returns a non-empty handle if the deleter intercepts the request.
 * The return value is true if the property could be deleted and false
 * otherwise.
 */
typedef Handle<Boolean> (*NamedPropertyDeleter)(Local<String> property,
                                                const AccessorInfo& info);

/**
 * Returns an array containing the names of the properties the named
 * property getter intercepts.
 */
typedef Handle<Array> (*NamedPropertyEnumerator)(const AccessorInfo& info);


/**
 * Returns the value of the property if the getter intercepts the
 * request.  Otherwise, returns an empty handle.
 */
typedef Handle<Value> (*IndexedPropertyGetter)(uint32_t index,
                                               const AccessorInfo& info);


/**
 * Returns the value if the setter intercepts the request.
 * Otherwise, returns an empty handle.
 */
typedef Handle<Value> (*IndexedPropertySetter)(uint32_t index,
                                               Local<Value> value,
                                               const AccessorInfo& info);


/**
 * Returns a non-empty handle if the interceptor intercepts the request.
 * The result is an integer encoding property attributes.
 */
typedef Handle<Integer> (*IndexedPropertyQuery)(uint32_t index,
                                                const AccessorInfo& info);

/**
 * Returns a non-empty handle if the deleter intercepts the request.
 * The return value is true if the property could be deleted and false
 * otherwise.
 */
typedef Handle<Boolean> (*IndexedPropertyDeleter)(uint32_t index,
                                                  const AccessorInfo& info);

/**
 * Returns an array containing the indices of the properties the
 * indexed property getter intercepts.
 */
typedef Handle<Array> (*IndexedPropertyEnumerator)(const AccessorInfo& info);


/**
 * Access type specification.
 */
enum AccessType {
  ACCESS_GET,
  ACCESS_SET,
  ACCESS_HAS,
  ACCESS_DELETE,
  ACCESS_KEYS
};


/**
 * Returns true if cross-context access should be allowed to the named
 * property with the given key on the host object.
 */
typedef bool (*NamedSecurityCallback)(Local<Object> host,
                                      Local<Value> key,
                                      AccessType type,
                                      Local<Value> data);


/**
 * Returns true if cross-context access should be allowed to the indexed
 * property with the given index on the host object.
 */
typedef bool (*IndexedSecurityCallback)(Local<Object> host,
                                        uint32_t index,
                                        AccessType type,
                                        Local<Value> data);


/**
 * A FunctionTemplate is used to create functions at runtime. There
 * can only be one function created from a FunctionTemplate in a
 * context.  The lifetime of the created function is equal to the
 * lifetime of the context.  So in case the embedder needs to create
 * temporary functions that can be collected using Scripts is
 * preferred.
 *
 * A FunctionTemplate can have properties, these properties are added to the
 * function object when it is created.
 *
 * A FunctionTemplate has a corresponding instance template which is
 * used to create object instances when the function is used as a
 * constructor. Properties added to the instance template are added to
 * each object instance.
 *
 * A FunctionTemplate can have a prototype template. The prototype template
 * is used to create the prototype object of the function.
 *
 * The following example shows how to use a FunctionTemplate:
 *
 * \code
 *    v8::Local<v8::FunctionTemplate> t = v8::FunctionTemplate::New();
 *    t->Set("func_property", v8::Number::New(1));
 *
 *    v8::Local<v8::Template> proto_t = t->PrototypeTemplate();
 *    proto_t->Set("proto_method", v8::FunctionTemplate::New(InvokeCallback));
 *    proto_t->Set("proto_const", v8::Number::New(2));
 *
 *    v8::Local<v8::ObjectTemplate> instance_t = t->InstanceTemplate();
 *    instance_t->SetAccessor("instance_accessor", InstanceAccessorCallback);
 *    instance_t->SetNamedPropertyHandler(PropertyHandlerCallback, ...);
 *    instance_t->Set("instance_property", Number::New(3));
 *
 *    v8::Local<v8::Function> function = t->GetFunction();
 *    v8::Local<v8::Object> instance = function->NewInstance();
 * \endcode
 *
 * Let's use "function" as the JS variable name of the function object
 * and "instance" for the instance object created above.  The function
 * and the instance will have the following properties:
 *
 * \code
 *   func_property in function == true;
 *   function.func_property == 1;
 *
 *   function.prototype.proto_method() invokes 'InvokeCallback'
 *   function.prototype.proto_const == 2;
 *
 *   instance instanceof function == true;
 *   instance.instance_accessor calls 'InstanceAccessorCallback'
 *   instance.instance_property == 3;
 * \endcode
 *
 * A FunctionTemplate can inherit from another one by calling the
 * FunctionTemplate::Inherit method.  The following graph illustrates
 * the semantics of inheritance:
 *
 * \code
 *   FunctionTemplate Parent  -> Parent() . prototype -> { }
 *     ^                                                  ^
 *     | Inherit(Parent)                                  | .__proto__
 *     |                                                  |
 *   FunctionTemplate Child   -> Child()  . prototype -> { }
 * \endcode
 *
 * A FunctionTemplate 'Child' inherits from 'Parent', the prototype
 * object of the Child() function has __proto__ pointing to the
 * Parent() function's prototype object. An instance of the Child
 * function has all properties on Parent's instance templates.
 *
 * Let Parent be the FunctionTemplate initialized in the previous
 * section and create a Child FunctionTemplate by:
 *
 * \code
 *   Local<FunctionTemplate> parent = t;
 *   Local<FunctionTemplate> child = FunctionTemplate::New();
 *   child->Inherit(parent);
 *
 *   Local<Function> child_function = child->GetFunction();
 *   Local<Object> child_instance = child_function->NewInstance();
 * \endcode
 *
 * The Child function and Child instance will have the following
 * properties:
 *
 * \code
 *   child_func.prototype.__proto__ == function.prototype;
 *   child_instance.instance_accessor calls 'InstanceAccessorCallback'
 *   child_instance.instance_property == 3;
 * \endcode
 */
class V8EXPORT FunctionTemplate : public Template {
 public:
  /** Creates a function template.*/
  static Local<FunctionTemplate> New(
      InvocationCallback callback = 0,
      Handle<Value> data = Handle<Value>(),
      Handle<Signature> signature = Handle<Signature>());
  /** Returns the unique function instance in the current execution context.*/
  Local<Function> GetFunction();

  /**
   * Set the call-handler callback for a FunctionTemplate.  This
   * callback is called whenever the function created from this
   * FunctionTemplate is called.
   */
  void SetCallHandler(InvocationCallback callback,
                      Handle<Value> data = Handle<Value>());

  /** Get the InstanceTemplate. */
  Local<ObjectTemplate> InstanceTemplate();

  /** Causes the function template to inherit from a parent function template.*/
  void Inherit(Handle<FunctionTemplate> parent);

  /**
   * A PrototypeTemplate is the template used to create the prototype object
   * of the function created by this template.
   */
  Local<ObjectTemplate> PrototypeTemplate();


  /**
   * Set the class name of the FunctionTemplate.  This is used for
   * printing objects created with the function created from the
   * FunctionTemplate as its constructor.
   */
  void SetClassName(Handle<String> name);

  /**
   * Determines whether the __proto__ accessor ignores instances of
   * the function template.  If instances of the function template are
   * ignored, __proto__ skips all instances and instead returns the
   * next object in the prototype chain.
   *
   * Call with a value of true to make the __proto__ accessor ignore
   * instances of the function template.  Call with a value of false
   * to make the __proto__ accessor not ignore instances of the
   * function template.  By default, instances of a function template
   * are not ignored.
   */
  void SetHiddenPrototype(bool value);

  /**
   * Returns true if the given object is an instance of this function
   * template.
   */
  bool HasInstance(Handle<Value> object);

 private:
  FunctionTemplate();
  void AddInstancePropertyAccessor(Handle<String> name,
                                   AccessorGetter getter,
                                   AccessorSetter setter,
                                   Handle<Value> data,
                                   AccessControl settings,
                                   PropertyAttribute attributes);
  void SetNamedInstancePropertyHandler(NamedPropertyGetter getter,
                                       NamedPropertySetter setter,
                                       NamedPropertyQuery query,
                                       NamedPropertyDeleter remover,
                                       NamedPropertyEnumerator enumerator,
                                       Handle<Value> data);
  void SetIndexedInstancePropertyHandler(IndexedPropertyGetter getter,
                                         IndexedPropertySetter setter,
                                         IndexedPropertyQuery query,
                                         IndexedPropertyDeleter remover,
                                         IndexedPropertyEnumerator enumerator,
                                         Handle<Value> data);
  void SetInstanceCallAsFunctionHandler(InvocationCallback callback,
                                        Handle<Value> data);

  friend class Context;
  friend class ObjectTemplate;
};


/**
 * An ObjectTemplate is used to create objects at runtime.
 *
 * Properties added to an ObjectTemplate are added to each object
 * created from the ObjectTemplate.
 */
class V8EXPORT ObjectTemplate : public Template {
 public:
  /** Creates an ObjectTemplate. */
  static Local<ObjectTemplate> New();

  /** Creates a new instance of this template.*/
  Local<Object> NewInstance();

  /**
   * Sets an accessor on the object template.
   *
   * Whenever the property with the given name is accessed on objects
   * created from this ObjectTemplate the getter and setter callbacks
   * are called instead of getting and setting the property directly
   * on the JavaScript object.
   *
   * \param name The name of the property for which an accessor is added.
   * \param getter The callback to invoke when getting the property.
   * \param setter The callback to invoke when setting the property.
   * \param data A piece of data that will be passed to the getter and setter
   *   callbacks whenever they are invoked.
   * \param settings Access control settings for the accessor. This is a bit
   *   field consisting of one of more of
   *   DEFAULT = 0, ALL_CAN_READ = 1, or ALL_CAN_WRITE = 2.
   *   The default is to not allow cross-context access.
   *   ALL_CAN_READ means that all cross-context reads are allowed.
   *   ALL_CAN_WRITE means that all cross-context writes are allowed.
   *   The combination ALL_CAN_READ | ALL_CAN_WRITE can be used to allow all
   *   cross-context access.
   * \param attribute The attributes of the property for which an accessor
   *   is added.
   */
  void SetAccessor(Handle<String> name,
                   AccessorGetter getter,
                   AccessorSetter setter = 0,
                   Handle<Value> data = Handle<Value>(),
                   AccessControl settings = DEFAULT,
                   PropertyAttribute attribute = None);

  /**
   * Sets a named property handler on the object template.
   *
   * Whenever a named property is accessed on objects created from
   * this object template, the provided callback is invoked instead of
   * accessing the property directly on the JavaScript object.
   *
   * \param getter The callback to invoke when getting a property.
   * \param setter The callback to invoke when setting a property.
   * \param query The callback to invoke to check if a property is present,
   *   and if present, get its attributes.
   * \param deleter The callback to invoke when deleting a property.
   * \param enumerator The callback to invoke to enumerate all the named
   *   properties of an object.
   * \param data A piece of data that will be passed to the callbacks
   *   whenever they are invoked.
   */
  void SetNamedPropertyHandler(NamedPropertyGetter getter,
                               NamedPropertySetter setter = 0,
                               NamedPropertyQuery query = 0,
                               NamedPropertyDeleter deleter = 0,
                               NamedPropertyEnumerator enumerator = 0,
                               Handle<Value> data = Handle<Value>());

  /**
   * Sets an indexed property handler on the object template.
   *
   * Whenever an indexed property is accessed on objects created from
   * this object template, the provided callback is invoked instead of
   * accessing the property directly on the JavaScript object.
   *
   * \param getter The callback to invoke when getting a property.
   * \param setter The callback to invoke when setting a property.
   * \param query The callback to invoke to check is an object has a property.
   * \param deleter The callback to invoke when deleting a property.
   * \param enumerator The callback to invoke to enumerate all the indexed
   *   properties of an object.
   * \param data A piece of data that will be passed to the callbacks
   *   whenever they are invoked.
   */
  void SetIndexedPropertyHandler(IndexedPropertyGetter getter,
                                 IndexedPropertySetter setter = 0,
                                 IndexedPropertyQuery query = 0,
                                 IndexedPropertyDeleter deleter = 0,
                                 IndexedPropertyEnumerator enumerator = 0,
                                 Handle<Value> data = Handle<Value>());

  /**
   * Sets the callback to be used when calling instances created from
   * this template as a function.  If no callback is set, instances
   * behave like normal JavaScript objects that cannot be called as a
   * function.
   */
  void SetCallAsFunctionHandler(InvocationCallback callback,
                                Handle<Value> data = Handle<Value>());

  /**
   * Mark object instances of the template as undetectable.
   *
   * In many ways, undetectable objects behave as though they are not
   * there.  They behave like 'undefined' in conditionals and when
   * printed.  However, properties can be accessed and called as on
   * normal objects.
   */
  void MarkAsUndetectable();

  /**
   * Sets access check callbacks on the object template.
   *
   * When accessing properties on instances of this object template,
   * the access check callback will be called to determine whether or
   * not to allow cross-context access to the properties.
   * The last parameter specifies whether access checks are turned
   * on by default on instances. If access checks are off by default,
   * they can be turned on on individual instances by calling
   * Object::TurnOnAccessCheck().
   */
  void SetAccessCheckCallbacks(NamedSecurityCallback named_handler,
                               IndexedSecurityCallback indexed_handler,
                               Handle<Value> data = Handle<Value>(),
                               bool turned_on_by_default = true);

  /**
   * Gets the number of internal fields for objects generated from
   * this template.
   */
  int InternalFieldCount();

  /**
   * Sets the number of internal fields for objects generated from
   * this template.
   */
  void SetInternalFieldCount(int value);

 private:
  ObjectTemplate();
  static Local<ObjectTemplate> New(Handle<FunctionTemplate> constructor);
  friend class FunctionTemplate;
};


/**
 * A Signature specifies which receivers and arguments a function can
 * legally be called with.
 */
class V8EXPORT Signature : public Data {
 public:
  static Local<Signature> New(Handle<FunctionTemplate> receiver =
                                  Handle<FunctionTemplate>(),
                              int argc = 0,
                              Handle<FunctionTemplate> argv[] = 0);
 private:
  Signature();
};


/**
 * A utility for determining the type of objects based on the template
 * they were constructed from.
 */
class V8EXPORT TypeSwitch : public Data {
 public:
  static Local<TypeSwitch> New(Handle<FunctionTemplate> type);
  static Local<TypeSwitch> New(int argc, Handle<FunctionTemplate> types[]);
  int match(Handle<Value> value);
 private:
  TypeSwitch();
};


// --- E x t e n s i o n s ---


/**
 * Ignore
 */
class V8EXPORT Extension {  // NOLINT
 public:
  Extension(const char* name,
            const char* source = 0,
            int dep_count = 0,
            const char** deps = 0);
  virtual ~Extension() { }
  virtual v8::Handle<v8::FunctionTemplate>
      GetNativeFunction(v8::Handle<v8::String> name) {
    return v8::Handle<v8::FunctionTemplate>();
  }

  const char* name() { return name_; }
  const char* source() { return source_; }
  int dependency_count() { return dep_count_; }
  const char** dependencies() { return deps_; }
  void set_auto_enable(bool value) { auto_enable_ = value; }
  bool auto_enable() { return auto_enable_; }

 private:
  const char* name_;
  const char* source_;
  int dep_count_;
  const char** deps_;
  bool auto_enable_;

  // Disallow copying and assigning.
  Extension(const Extension&);
  void operator=(const Extension&);
};


void V8EXPORT RegisterExtension(Extension* extension);


/**
 * Ignore
 */
class V8EXPORT DeclareExtension {
 public:
  inline DeclareExtension(Extension* extension) {
    RegisterExtension(extension);
  }
};


// --- S t a t i c s ---


Handle<Primitive> V8EXPORT Undefined();
Handle<Primitive> V8EXPORT Null();
Handle<Boolean> V8EXPORT True();
Handle<Boolean> V8EXPORT False();


/**
 * A set of constraints that specifies the limits of the runtime's memory use.
 * You must set the heap size before initializing the VM - the size cannot be
 * adjusted after the VM is initialized.
 *
 * If you are using threads then you should hold the V8::Locker lock while
 * setting the stack limit and you must set a non-default stack limit separately
 * for each thread.
 */
class V8EXPORT ResourceConstraints {
 public:
  ResourceConstraints();
  int max_young_space_size() const { return max_young_space_size_; }
  void set_max_young_space_size(int value) { max_young_space_size_ = value; }
  int max_old_space_size() const { return max_old_space_size_; }
  void set_max_old_space_size(int value) { max_old_space_size_ = value; }
  uint32_t* stack_limit() const { return stack_limit_; }
  // Sets an address beyond which the VM's stack may not grow.
  void set_stack_limit(uint32_t* value) { stack_limit_ = value; }
 private:
  int max_young_space_size_;
  int max_old_space_size_;
  uint32_t* stack_limit_;
};


bool V8EXPORT SetResourceConstraints(ResourceConstraints* constraints);


// --- E x c e p t i o n s ---


typedef void (*FatalErrorCallback)(const char* location, const char* message);


typedef void (*MessageCallback)(Handle<Message> message, Handle<Value> data);


/**
 * Schedules an exception to be thrown when returning to JavaScript.  When an
 * exception has been scheduled it is illegal to invoke any JavaScript
 * operation; the caller must return immediately and only after the exception
 * has been handled does it become legal to invoke JavaScript operations.
 */
Handle<Value> V8EXPORT ThrowException(Handle<Value> exception);

/**
 * Create new error objects by calling the corresponding error object
 * constructor with the message.
 */
class V8EXPORT Exception {
 public:
  static Local<Value> RangeError(Handle<String> message);
  static Local<Value> ReferenceError(Handle<String> message);
  static Local<Value> SyntaxError(Handle<String> message);
  static Local<Value> TypeError(Handle<String> message);
  static Local<Value> Error(Handle<String> message);
};


// --- C o u n t e r s  C a l l b a c k s ---

typedef int* (*CounterLookupCallback)(const char* name);

typedef void* (*CreateHistogramCallback)(const char* name,
                                         int min,
                                         int max,
                                         size_t buckets);

typedef void (*AddHistogramSampleCallback)(void* histogram, int sample);

// --- M e m o r y  A l l o c a t i o n   C a l l b a c k ---
  enum ObjectSpace {
    kObjectSpaceNewSpace = 1 << 0,
    kObjectSpaceOldPointerSpace = 1 << 1,
    kObjectSpaceOldDataSpace = 1 << 2,
    kObjectSpaceCodeSpace = 1 << 3,
    kObjectSpaceMapSpace = 1 << 4,
    kObjectSpaceLoSpace = 1 << 5,

    kObjectSpaceAll = kObjectSpaceNewSpace | kObjectSpaceOldPointerSpace |
      kObjectSpaceOldDataSpace | kObjectSpaceCodeSpace | kObjectSpaceMapSpace |
      kObjectSpaceLoSpace
  };

  enum AllocationAction {
    kAllocationActionAllocate = 1 << 0,
    kAllocationActionFree = 1 << 1,
    kAllocationActionAll = kAllocationActionAllocate | kAllocationActionFree
  };

typedef void (*MemoryAllocationCallback)(ObjectSpace space,
                                         AllocationAction action,
                                         int size);

// --- F a i l e d A c c e s s C h e c k C a l l b a c k ---
typedef void (*FailedAccessCheckCallback)(Local<Object> target,
                                          AccessType type,
                                          Local<Value> data);

// --- G a r b a g e C o l l e c t i o n  C a l l b a c k s

/**
 * Applications can register callback functions which will be called
 * before and after a garbage collection.  Allocations are not
 * allowed in the callback functions, you therefore cannot manipulate
 * objects (set or delete properties for example) since it is possible
 * such operations will result in the allocation of objects.
 */
enum GCType {
  kGCTypeScavenge = 1 << 0,
  kGCTypeMarkSweepCompact = 1 << 1,
  kGCTypeAll = kGCTypeScavenge | kGCTypeMarkSweepCompact
};

enum GCCallbackFlags {
  kNoGCCallbackFlags = 0,
  kGCCallbackFlagCompacted = 1 << 0
};

typedef void (*GCPrologueCallback)(GCType type, GCCallbackFlags flags);
typedef void (*GCEpilogueCallback)(GCType type, GCCallbackFlags flags);

typedef void (*GCCallback)();


/**
 * Profiler modules.
 *
 * In V8, profiler consists of several modules: CPU profiler, and different
 * kinds of heap profiling. Each can be turned on / off independently.
 * When PROFILER_MODULE_HEAP_SNAPSHOT flag is passed to ResumeProfilerEx,
 * modules are enabled only temporarily for making a snapshot of the heap.
 */
enum ProfilerModules {
  PROFILER_MODULE_NONE            = 0,
  PROFILER_MODULE_CPU             = 1,
  PROFILER_MODULE_HEAP_STATS      = 1 << 1,
  PROFILER_MODULE_JS_CONSTRUCTORS = 1 << 2,
  PROFILER_MODULE_HEAP_SNAPSHOT   = 1 << 16
};


/**
 * Collection of V8 heap information.
 *
 * Instances of this class can be passed to v8::V8::HeapStatistics to
 * get heap statistics from V8.
 */
class V8EXPORT HeapStatistics {
 public:
  HeapStatistics();
  size_t total_heap_size() { return total_heap_size_; }
  size_t used_heap_size() { return used_heap_size_; }

 private:
  void set_total_heap_size(size_t size) { total_heap_size_ = size; }
  void set_used_heap_size(size_t size) { used_heap_size_ = size; }

  size_t total_heap_size_;
  size_t used_heap_size_;

  friend class V8;
};


/**
 * Container class for static utility functions.
 */
class V8EXPORT V8 {
 public:
  /** Set the callback to invoke in case of fatal errors. */
  static void SetFatalErrorHandler(FatalErrorCallback that);

  /**
   * Ignore out-of-memory exceptions.
   *
   * V8 running out of memory is treated as a fatal error by default.
   * This means that the fatal error handler is called and that V8 is
   * terminated.
   *
   * IgnoreOutOfMemoryException can be used to not treat a
   * out-of-memory situation as a fatal error.  This way, the contexts
   * that did not cause the out of memory problem might be able to
   * continue execution.
   */
  static void IgnoreOutOfMemoryException();

  /**
   * Check if V8 is dead and therefore unusable.  This is the case after
   * fatal errors such as out-of-memory situations.
   */
  static bool IsDead();

  /**
   * Adds a message listener.
   *
   * The same message listener can be added more than once and it that
   * case it will be called more than once for each message.
   */
  static bool AddMessageListener(MessageCallback that,
                                 Handle<Value> data = Handle<Value>());

  /**
   * Remove all message listeners from the specified callback function.
   */
  static void RemoveMessageListeners(MessageCallback that);

  /**
   * Tells V8 to capture current stack trace when uncaught exception occurs
   * and report it to the message listeners. The option is off by default.
   */
  static void SetCaptureStackTraceForUncaughtExceptions(
      bool capture,
      int frame_limit = 10,
      StackTrace::StackTraceOptions options = StackTrace::kOverview);

  /**
   * Sets V8 flags from a string.
   */
  static void SetFlagsFromString(const char* str, int length);

  /**
   * Sets V8 flags from the command line.
   */
  static void SetFlagsFromCommandLine(int* argc,
                                      char** argv,
                                      bool remove_flags);

  /** Get the version string. */
  static const char* GetVersion();

  /**
   * Enables the host application to provide a mechanism for recording
   * statistics counters.
   */
  static void SetCounterFunction(CounterLookupCallback);

  /**
   * Enables the host application to provide a mechanism for recording
   * histograms. The CreateHistogram function returns a
   * histogram which will later be passed to the AddHistogramSample
   * function.
   */
  static void SetCreateHistogramFunction(CreateHistogramCallback);
  static void SetAddHistogramSampleFunction(AddHistogramSampleCallback);

  /**
   * Enables the computation of a sliding window of states. The sliding
   * window information is recorded in statistics counters.
   */
  static void EnableSlidingStateWindow();

  /** Callback function for reporting failed access checks.*/
  static void SetFailedAccessCheckCallbackFunction(FailedAccessCheckCallback);

  /**
   * Enables the host application to receive a notification before a
   * garbage collection.  Allocations are not allowed in the
   * callback function, you therefore cannot manipulate objects (set
   * or delete properties for example) since it is possible such
   * operations will result in the allocation of objects. It is possible
   * to specify the GCType filter for your callback. But it is not possible to
   * register the same callback function two times with different
   * GCType filters.
   */
  static void AddGCPrologueCallback(
      GCPrologueCallback callback, GCType gc_type_filter = kGCTypeAll);

  /**
   * This function removes callback which was installed by
   * AddGCPrologueCallback function.
   */
  static void RemoveGCPrologueCallback(GCPrologueCallback callback);

  /**
   * The function is deprecated. Please use AddGCPrologueCallback instead.
   * Enables the host application to receive a notification before a
   * garbage collection.  Allocations are not allowed in the
   * callback function, you therefore cannot manipulate objects (set
   * or delete properties for example) since it is possible such
   * operations will result in the allocation of objects.
   */
  static void SetGlobalGCPrologueCallback(GCCallback);

  /**
   * Enables the host application to receive a notification after a
   * garbage collection.  Allocations are not allowed in the
   * callback function, you therefore cannot manipulate objects (set
   * or delete properties for example) since it is possible such
   * operations will result in the allocation of objects. It is possible
   * to specify the GCType filter for your callback. But it is not possible to
   * register the same callback function two times with different
   * GCType filters.
   */
  static void AddGCEpilogueCallback(
      GCEpilogueCallback callback, GCType gc_type_filter = kGCTypeAll);

  /**
   * This function removes callback which was installed by
   * AddGCEpilogueCallback function.
   */
  static void RemoveGCEpilogueCallback(GCEpilogueCallback callback);

  /**
   * The function is deprecated. Please use AddGCEpilogueCallback instead.
   * Enables the host application to receive a notification after a
   * major garbage collection.  Allocations are not allowed in the
   * callback function, you therefore cannot manipulate objects (set
   * or delete properties for example) since it is possible such
   * operations will result in the allocation of objects.
   */
  static void SetGlobalGCEpilogueCallback(GCCallback);

  /**
   * Enables the host application to provide a mechanism to be notified
   * and perform custom logging when V8 Allocates Executable Memory.
   */
  static void AddMemoryAllocationCallback(MemoryAllocationCallback callback,
                                          ObjectSpace space,
                                          AllocationAction action);

  /**
   * This function removes callback which was installed by
   * AddMemoryAllocationCallback function.
   */
  static void RemoveMemoryAllocationCallback(MemoryAllocationCallback callback);

  /**
   * Allows the host application to group objects together. If one
   * object in the group is alive, all objects in the group are alive.
   * After each garbage collection, object groups are removed. It is
   * intended to be used in the before-garbage-collection callback
   * function, for instance to simulate DOM tree connections among JS
   * wrapper objects.
   */
  static void AddObjectGroup(Persistent<Value>* objects, size_t length);

  /**
   * Initializes from snapshot if possible. Otherwise, attempts to
   * initialize from scratch.  This function is called implicitly if
   * you use the API without calling it first.
   */
  static bool Initialize();

  /**
   * Adjusts the amount of registered external memory.  Used to give
   * V8 an indication of the amount of externally allocated memory
   * that is kept alive by JavaScript objects.  V8 uses this to decide
   * when to perform global garbage collections.  Registering
   * externally allocated memory will trigger global garbage
   * collections more often than otherwise in an attempt to garbage
   * collect the JavaScript objects keeping the externally allocated
   * memory alive.
   *
   * \param change_in_bytes the change in externally allocated memory
   *   that is kept alive by JavaScript objects.
   * \returns the adjusted value.
   */
  static int AdjustAmountOfExternalAllocatedMemory(int change_in_bytes);

  /**
   * Suspends recording of tick samples in the profiler.
   * When the V8 profiling mode is enabled (usually via command line
   * switches) this function suspends recording of tick samples.
   * Profiling ticks are discarded until ResumeProfiler() is called.
   *
   * See also the --prof and --prof_auto command line switches to
   * enable V8 profiling.
   */
  static void PauseProfiler();

  /**
   * Resumes recording of tick samples in the profiler.
   * See also PauseProfiler().
   */
  static void ResumeProfiler();

  /**
   * Return whether profiler is currently paused.
   */
  static bool IsProfilerPaused();

  /**
   * Resumes specified profiler modules. Can be called several times to
   * mark the opening of a profiler events block with the given tag.
   *
   * "ResumeProfiler" is equivalent to "ResumeProfilerEx(PROFILER_MODULE_CPU)".
   * See ProfilerModules enum.
   *
   * \param flags Flags specifying profiler modules.
   * \param tag Profile tag.
   */
  static void ResumeProfilerEx(int flags, int tag = 0);

  /**
   * Pauses specified profiler modules. Each call to "PauseProfilerEx" closes
   * a block of profiler events opened by a call to "ResumeProfilerEx" with the
   * same tag value. There is no need for blocks to be properly nested.
   * The profiler is paused when the last opened block is closed.
   *
   * "PauseProfiler" is equivalent to "PauseProfilerEx(PROFILER_MODULE_CPU)".
   * See ProfilerModules enum.
   *
   * \param flags Flags specifying profiler modules.
   * \param tag Profile tag.
   */
  static void PauseProfilerEx(int flags, int tag = 0);

  /**
   * Returns active (resumed) profiler modules.
   * See ProfilerModules enum.
   *
   * \returns active profiler modules.
   */
  static int GetActiveProfilerModules();

  /**
   * If logging is performed into a memory buffer (via --logfile=*), allows to
   * retrieve previously written messages. This can be used for retrieving
   * profiler log data in the application. This function is thread-safe.
   *
   * Caller provides a destination buffer that must exist during GetLogLines
   * call. Only whole log lines are copied into the buffer.
   *
   * \param from_pos specified a point in a buffer to read from, 0 is the
   *   beginning of a buffer. It is assumed that caller updates its current
   *   position using returned size value from the previous call.
   * \param dest_buf destination buffer for log data.
   * \param max_size size of the destination buffer.
   * \returns actual size of log data copied into buffer.
   */
  static int GetLogLines(int from_pos, char* dest_buf, int max_size);

  /**
   * The minimum allowed size for a log lines buffer.  If the size of
   * the buffer given will not be enough to hold a line of the maximum
   * length, an attempt to find a log line end in GetLogLines will
   * fail, and an empty result will be returned.
   */
  static const int kMinimumSizeForLogLinesBuffer = 2048;

  /**
   * Retrieve the V8 thread id of the calling thread.
   *
   * The thread id for a thread should only be retrieved after the V8
   * lock has been acquired with a Locker object with that thread.
   */
  static int GetCurrentThreadId();

  /**
   * Forcefully terminate execution of a JavaScript thread.  This can
   * be used to terminate long-running scripts.
   *
   * TerminateExecution should only be called when then V8 lock has
   * been acquired with a Locker object.  Therefore, in order to be
   * able to terminate long-running threads, preemption must be
   * enabled to allow the user of TerminateExecution to acquire the
   * lock.
   *
   * The termination is achieved by throwing an exception that is
   * uncatchable by JavaScript exception handlers.  Termination
   * exceptions act as if they were caught by a C++ TryCatch exception
   * handlers.  If forceful termination is used, any C++ TryCatch
   * exception handler that catches an exception should check if that
   * exception is a termination exception and immediately return if
   * that is the case.  Returning immediately in that case will
   * continue the propagation of the termination exception if needed.
   *
   * The thread id passed to TerminateExecution must have been
   * obtained by calling GetCurrentThreadId on the thread in question.
   *
   * \param thread_id The thread id of the thread to terminate.
   */
  static void TerminateExecution(int thread_id);

  /**
   * Forcefully terminate the current thread of JavaScript execution.
   *
   * This method can be used by any thread even if that thread has not
   * acquired the V8 lock with a Locker object.
   */
  static void TerminateExecution();

  /**
   * Is V8 terminating JavaScript execution.
   *
   * Returns true if JavaScript execution is currently terminating
   * because of a call to TerminateExecution.  In that case there are
   * still JavaScript frames on the stack and the termination
   * exception is still active.
   */
  static bool IsExecutionTerminating();

  /**
   * Releases any resources used by v8 and stops any utility threads
   * that may be running.  Note that disposing v8 is permanent, it
   * cannot be reinitialized.
   *
   * It should generally not be necessary to dispose v8 before exiting
   * a process, this should happen automatically.  It is only necessary
   * to use if the process needs the resources taken up by v8.
   */
  static bool Dispose();

  /**
   * Get statistics about the heap memory usage.
   */
  static void GetHeapStatistics(HeapStatistics* heap_statistics);

  /**
   * Optional notification that the embedder is idle.
   * V8 uses the notification to reduce memory footprint.
   * This call can be used repeatedly if the embedder remains idle.
   * Returns true if the embedder should stop calling IdleNotification
   * until real work has been done.  This indicates that V8 has done
   * as much cleanup as it will be able to do.
   */
  static bool IdleNotification();

  /**
   * Optional notification that the system is running low on memory.
   * V8 uses these notifications to attempt to free memory.
   */
  static void LowMemoryNotification();

  /**
   * Optional notification that a context has been disposed. V8 uses
   * these notifications to guide the GC heuristic. Returns the number
   * of context disposals - including this one - since the last time
   * V8 had a chance to clean up.
   */
  static int ContextDisposedNotification();

 private:
  V8();

  static internal::Object** GlobalizeReference(internal::Object** handle);
  static void DisposeGlobal(internal::Object** global_handle);
  static void MakeWeak(internal::Object** global_handle,
                       void* data,
                       WeakReferenceCallback);
  static void ClearWeak(internal::Object** global_handle);
  static bool IsGlobalNearDeath(internal::Object** global_handle);
  static bool IsGlobalWeak(internal::Object** global_handle);

  template <class T> friend class Handle;
  template <class T> friend class Local;
  template <class T> friend class Persistent;
  friend class Context;
};


/**
 * An external exception handler.
 */
class V8EXPORT TryCatch {
 public:

  /**
   * Creates a new try/catch block and registers it with v8.
   */
  TryCatch();

  /**
   * Unregisters and deletes this try/catch block.
   */
  ~TryCatch();

  /**
   * Returns true if an exception has been caught by this try/catch block.
   */
  bool HasCaught() const;

  /**
   * For certain types of exceptions, it makes no sense to continue
   * execution.
   *
   * Currently, the only type of exception that can be caught by a
   * TryCatch handler and for which it does not make sense to continue
   * is termination exception.  Such exceptions are thrown when the
   * TerminateExecution methods are called to terminate a long-running
   * script.
   *
   * If CanContinue returns false, the correct action is to perform
   * any C++ cleanup needed and then return.
   */
  bool CanContinue() const;

  /**
   * Throws the exception caught by this TryCatch in a way that avoids
   * it being caught again by this same TryCatch.  As with ThrowException
   * it is illegal to execute any JavaScript operations after calling
   * ReThrow; the caller must return immediately to where the exception
   * is caught.
   */
  Handle<Value> ReThrow();

  /**
   * Returns the exception caught by this try/catch block.  If no exception has
   * been caught an empty handle is returned.
   *
   * The returned handle is valid until this TryCatch block has been destroyed.
   */
  Local<Value> Exception() const;

  /**
   * Returns the .stack property of the thrown object.  If no .stack
   * property is present an empty handle is returned.
   */
  Local<Value> StackTrace() const;

  /**
   * Returns the message associated with this exception.  If there is
   * no message associated an empty handle is returned.
   *
   * The returned handle is valid until this TryCatch block has been
   * destroyed.
   */
  Local<v8::Message> Message() const;

  /**
   * Clears any exceptions that may have been caught by this try/catch block.
   * After this method has been called, HasCaught() will return false.
   *
   * It is not necessary to clear a try/catch block before using it again; if
   * another exception is thrown the previously caught exception will just be
   * overwritten.  However, it is often a good idea since it makes it easier
   * to determine which operation threw a given exception.
   */
  void Reset();

  /**
   * Set verbosity of the external exception handler.
   *
   * By default, exceptions that are caught by an external exception
   * handler are not reported.  Call SetVerbose with true on an
   * external exception handler to have exceptions caught by the
   * handler reported as if they were not caught.
   */
  void SetVerbose(bool value);

  /**
   * Set whether or not this TryCatch should capture a Message object
   * which holds source information about where the exception
   * occurred.  True by default.
   */
  void SetCaptureMessage(bool value);

 private:
  void* next_;
  void* exception_;
  void* message_;
  bool is_verbose_ : 1;
  bool can_continue_ : 1;
  bool capture_message_ : 1;
  bool rethrow_ : 1;

  friend class v8::internal::Top;
};


// --- C o n t e x t ---


/**
 * Ignore
 */
class V8EXPORT ExtensionConfiguration {
 public:
  ExtensionConfiguration(int name_count, const char* names[])
      : name_count_(name_count), names_(names) { }
 private:
  friend class ImplementationUtilities;
  int name_count_;
  const char** names_;
};


/**
 * A sandboxed execution context with its own set of built-in objects
 * and functions.
 */
class V8EXPORT Context {
 public:
  /** Returns the global object of the context. */
  Local<Object> Global();

  /**
   * Detaches the global object from its context before
   * the global object can be reused to create a new context.
   */
  void DetachGlobal();

  /**
   * Reattaches a global object to a context.  This can be used to
   * restore the connection between a global object and a context
   * after DetachGlobal has been called.
   *
   * \param global_object The global object to reattach to the
   *   context.  For this to work, the global object must be the global
   *   object that was associated with this context before a call to
   *   DetachGlobal.
   */
  void ReattachGlobal(Handle<Object> global_object);

  /** Creates a new context.
   *
   * Returns a persistent handle to the newly allocated context. This
   * persistent handle has to be disposed when the context is no
   * longer used so the context can be garbage collected.
   */
  static Persistent<Context> New(
      ExtensionConfiguration* extensions = NULL,
      Handle<ObjectTemplate> global_template = Handle<ObjectTemplate>(),
      Handle<Value> global_object = Handle<Value>());

  /** Returns the last entered context. */
  static Local<Context> GetEntered();

  /** Returns the context that is on the top of the stack. */
  static Local<Context> GetCurrent();

  /**
   * Returns the context of the calling JavaScript code.  That is the
   * context of the top-most JavaScript frame.  If there are no
   * JavaScript frames an empty handle is returned.
   */
  static Local<Context> GetCalling();

  /**
   * Sets the security token for the context.  To access an object in
   * another context, the security tokens must match.
   */
  void SetSecurityToken(Handle<Value> token);

  /** Restores the security token to the default value. */
  void UseDefaultSecurityToken();

  /** Returns the security token of this context.*/
  Handle<Value> GetSecurityToken();

  /**
   * Enter this context.  After entering a context, all code compiled
   * and run is compiled and run in this context.  If another context
   * is already entered, this old context is saved so it can be
   * restored when the new context is exited.
   */
  void Enter();

  /**
   * Exit this context.  Exiting the current context restores the
   * context that was in place when entering the current context.
   */
  void Exit();

  /** Returns true if the context has experienced an out of memory situation. */
  bool HasOutOfMemoryException();

  /** Returns true if V8 has a current context. */
  static bool InContext();

  /**
   * Associate an additional data object with the context. This is mainly used
   * with the debugger to provide additional information on the context through
   * the debugger API.
   */
  void SetData(Handle<String> data);
  Local<Value> GetData();

  /**
   * Stack-allocated class which sets the execution context for all
   * operations executed within a local scope.
   */
  class Scope {
   public:
    inline Scope(Handle<Context> context) : context_(context) {
      context_->Enter();
    }
    inline ~Scope() { context_->Exit(); }
   private:
    Handle<Context> context_;
  };

 private:
  friend class Value;
  friend class Script;
  friend class Object;
  friend class Function;
};


/**
 * Multiple threads in V8 are allowed, but only one thread at a time
 * is allowed to use V8.  The definition of 'using V8' includes
 * accessing handles or holding onto object pointers obtained from V8
 * handles.  It is up to the user of V8 to ensure (perhaps with
 * locking) that this constraint is not violated.
 *
 * If you wish to start using V8 in a thread you can do this by constructing
 * a v8::Locker object.  After the code using V8 has completed for the
 * current thread you can call the destructor.  This can be combined
 * with C++ scope-based construction as follows:
 *
 * \code
 * ...
 * {
 *   v8::Locker locker;
 *   ...
 *   // Code using V8 goes here.
 *   ...
 * } // Destructor called here
 * \endcode
 *
 * If you wish to stop using V8 in a thread A you can do this by either
 * by destroying the v8::Locker object as above or by constructing a
 * v8::Unlocker object:
 *
 * \code
 * {
 *   v8::Unlocker unlocker;
 *   ...
 *   // Code not using V8 goes here while V8 can run in another thread.
 *   ...
 * } // Destructor called here.
 * \endcode
 *
 * The Unlocker object is intended for use in a long-running callback
 * from V8, where you want to release the V8 lock for other threads to
 * use.
 *
 * The v8::Locker is a recursive lock.  That is, you can lock more than
 * once in a given thread.  This can be useful if you have code that can
 * be called either from code that holds the lock or from code that does
 * not.  The Unlocker is not recursive so you can not have several
 * Unlockers on the stack at once, and you can not use an Unlocker in a
 * thread that is not inside a Locker's scope.
 *
 * An unlocker will unlock several lockers if it has to and reinstate
 * the correct depth of locking on its destruction. eg.:
 *
 * \code
 * // V8 not locked.
 * {
 *   v8::Locker locker;
 *   // V8 locked.
 *   {
 *     v8::Locker another_locker;
 *     // V8 still locked (2 levels).
 *     {
 *       v8::Unlocker unlocker;
 *       // V8 not locked.
 *     }
 *     // V8 locked again (2 levels).
 *   }
 *   // V8 still locked (1 level).
 * }
 * // V8 Now no longer locked.
 * \endcode
 */
class V8EXPORT Unlocker {
 public:
  Unlocker();
  ~Unlocker();
};


class V8EXPORT Locker {
 public:
  Locker();
  ~Locker();

  /**
   * Start preemption.
   *
   * When preemption is started, a timer is fired every n milli seconds
   * that will switch between multiple threads that are in contention
   * for the V8 lock.
   */
  static void StartPreemption(int every_n_ms);

  /**
   * Stop preemption.
   */
  static void StopPreemption();

  /**
   * Returns whether or not the locker is locked by the current thread.
   */
  static bool IsLocked();

  /**
   * Returns whether v8::Locker is being used by this V8 instance.
   */
  static bool IsActive() { return active_; }

 private:
  bool has_lock_;
  bool top_level_;

  static bool active_;

  // Disallow copying and assigning.
  Locker(const Locker&);
  void operator=(const Locker&);
};



// --- I m p l e m e n t a t i o n ---


namespace internal {


// Tag information for HeapObject.
const int kHeapObjectTag = 1;
const int kHeapObjectTagSize = 2;
const intptr_t kHeapObjectTagMask = (1 << kHeapObjectTagSize) - 1;

// Tag information for Smi.
const int kSmiTag = 0;
const int kSmiTagSize = 1;
const intptr_t kSmiTagMask = (1 << kSmiTagSize) - 1;

template <size_t ptr_size> struct SmiConstants;

// Smi constants for 32-bit systems.
template <> struct SmiConstants<4> {
  static const int kSmiShiftSize = 0;
  static const int kSmiValueSize = 31;
  static inline int SmiToInt(internal::Object* value) {
    int shift_bits = kSmiTagSize + kSmiShiftSize;
    // Throw away top 32 bits and shift down (requires >> to be sign extending).
    return static_cast<int>(reinterpret_cast<intptr_t>(value)) >> shift_bits;
  }
};

// Smi constants for 64-bit systems.
template <> struct SmiConstants<8> {
  static const int kSmiShiftSize = 31;
  static const int kSmiValueSize = 32;
  static inline int SmiToInt(internal::Object* value) {
    int shift_bits = kSmiTagSize + kSmiShiftSize;
    // Shift down and throw away top 32 bits.
    return static_cast<int>(reinterpret_cast<intptr_t>(value) >> shift_bits);
  }
};

const int kSmiShiftSize = SmiConstants<sizeof(void*)>::kSmiShiftSize;
const int kSmiValueSize = SmiConstants<sizeof(void*)>::kSmiValueSize;

template <size_t ptr_size> struct InternalConstants;

// Internal constants for 32-bit systems.
template <> struct InternalConstants<4> {
  static const int kStringResourceOffset = 3 * sizeof(void*);
};

// Internal constants for 64-bit systems.
template <> struct InternalConstants<8> {
  static const int kStringResourceOffset = 3 * sizeof(void*);
};

/**
 * This class exports constants and functionality from within v8 that
 * is necessary to implement inline functions in the v8 api.  Don't
 * depend on functions and constants defined here.
 */
class Internals {
 public:

  // These values match non-compiler-dependent values defined within
  // the implementation of v8.
  static const int kHeapObjectMapOffset = 0;
  static const int kMapInstanceTypeOffset = sizeof(void*) + sizeof(int);
  static const int kStringResourceOffset =
      InternalConstants<sizeof(void*)>::kStringResourceOffset;

  static const int kProxyProxyOffset = sizeof(void*);
  static const int kJSObjectHeaderSize = 3 * sizeof(void*);
  static const int kFullStringRepresentationMask = 0x07;
  static const int kExternalTwoByteRepresentationTag = 0x02;

  static const int kJSObjectType = 0x9f;
  static const int kFirstNonstringType = 0x80;
  static const int kProxyType = 0x85;

  static inline bool HasHeapObjectTag(internal::Object* value) {
    return ((reinterpret_cast<intptr_t>(value) & kHeapObjectTagMask) ==
            kHeapObjectTag);
  }

  static inline bool HasSmiTag(internal::Object* value) {
    return ((reinterpret_cast<intptr_t>(value) & kSmiTagMask) == kSmiTag);
  }

  static inline int SmiValue(internal::Object* value) {
    return SmiConstants<sizeof(void*)>::SmiToInt(value);
  }

  static inline int GetInstanceType(internal::Object* obj) {
    typedef internal::Object O;
    O* map = ReadField<O*>(obj, kHeapObjectMapOffset);
    return ReadField<uint8_t>(map, kMapInstanceTypeOffset);
  }

  static inline void* GetExternalPointer(internal::Object* obj) {
    if (HasSmiTag(obj)) {
      return obj;
    } else if (GetInstanceType(obj) == kProxyType) {
      return ReadField<void*>(obj, kProxyProxyOffset);
    } else {
      return NULL;
    }
  }

  static inline bool IsExternalTwoByteString(int instance_type) {
    int representation = (instance_type & kFullStringRepresentationMask);
    return representation == kExternalTwoByteRepresentationTag;
  }

  template <typename T>
  static inline T ReadField(Object* ptr, int offset) {
    uint8_t* addr = reinterpret_cast<uint8_t*>(ptr) + offset - kHeapObjectTag;
    return *reinterpret_cast<T*>(addr);
  }

};

}


template <class T>
Handle<T>::Handle() : val_(0) { }


template <class T>
Local<T>::Local() : Handle<T>() { }


template <class T>
Local<T> Local<T>::New(Handle<T> that) {
  if (that.IsEmpty()) return Local<T>();
  internal::Object** p = reinterpret_cast<internal::Object**>(*that);
  return Local<T>(reinterpret_cast<T*>(HandleScope::CreateHandle(*p)));
}


template <class T>
Persistent<T> Persistent<T>::New(Handle<T> that) {
  if (that.IsEmpty()) return Persistent<T>();
  internal::Object** p = reinterpret_cast<internal::Object**>(*that);
  return Persistent<T>(reinterpret_cast<T*>(V8::GlobalizeReference(p)));
}


template <class T>
bool Persistent<T>::IsNearDeath() const {
  if (this->IsEmpty()) return false;
  return V8::IsGlobalNearDeath(reinterpret_cast<internal::Object**>(**this));
}


template <class T>
bool Persistent<T>::IsWeak() const {
  if (this->IsEmpty()) return false;
  return V8::IsGlobalWeak(reinterpret_cast<internal::Object**>(**this));
}


template <class T>
void Persistent<T>::Dispose() {
  if (this->IsEmpty()) return;
  V8::DisposeGlobal(reinterpret_cast<internal::Object**>(**this));
}


template <class T>
Persistent<T>::Persistent() : Handle<T>() { }

template <class T>
void Persistent<T>::MakeWeak(void* parameters, WeakReferenceCallback callback) {
  V8::MakeWeak(reinterpret_cast<internal::Object**>(**this),
               parameters,
               callback);
}

template <class T>
void Persistent<T>::ClearWeak() {
  V8::ClearWeak(reinterpret_cast<internal::Object**>(**this));
}


Arguments::Arguments(v8::Local<v8::Value> data,
                     v8::Local<v8::Object> holder,
                     v8::Local<v8::Function> callee,
                     bool is_construct_call,
                     void** values, int length)
    : data_(data), holder_(holder), callee_(callee),
      is_construct_call_(is_construct_call),
      values_(values), length_(length) { }


Local<Value> Arguments::operator[](int i) const {
  if (i < 0 || length_ <= i) return Local<Value>(*Undefined());
  return Local<Value>(reinterpret_cast<Value*>(values_ - i));
}


Local<Function> Arguments::Callee() const {
  return callee_;
}


Local<Object> Arguments::This() const {
  return Local<Object>(reinterpret_cast<Object*>(values_ + 1));
}


Local<Object> Arguments::Holder() const {
  return holder_;
}


Local<Value> Arguments::Data() const {
  return data_;
}


bool Arguments::IsConstructCall() const {
  return is_construct_call_;
}


int Arguments::Length() const {
  return length_;
}


template <class T>
Local<T> HandleScope::Close(Handle<T> value) {
  internal::Object** before = reinterpret_cast<internal::Object**>(*value);
  internal::Object** after = RawClose(before);
  return Local<T>(reinterpret_cast<T*>(after));
}

Handle<Value> ScriptOrigin::ResourceName() const {
  return resource_name_;
}


Handle<Integer> ScriptOrigin::ResourceLineOffset() const {
  return resource_line_offset_;
}


Handle<Integer> ScriptOrigin::ResourceColumnOffset() const {
  return resource_column_offset_;
}


Handle<Boolean> Boolean::New(bool value) {
  return value ? True() : False();
}


void Template::Set(const char* name, v8::Handle<Data> value) {
  Set(v8::String::New(name), value);
}


Local<Value> Object::GetInternalField(int index) {
#ifndef V8_ENABLE_CHECKS
  Local<Value> quick_result = UncheckedGetInternalField(index);
  if (!quick_result.IsEmpty()) return quick_result;
#endif
  return CheckedGetInternalField(index);
}


Local<Value> Object::UncheckedGetInternalField(int index) {
  typedef internal::Object O;
  typedef internal::Internals I;
  O* obj = *reinterpret_cast<O**>(this);
  if (I::GetInstanceType(obj) == I::kJSObjectType) {
    // If the object is a plain JSObject, which is the common case,
    // we know where to find the internal fields and can return the
    // value directly.
    int offset = I::kJSObjectHeaderSize + (sizeof(void*) * index);
    O* value = I::ReadField<O*>(obj, offset);
    O** result = HandleScope::CreateHandle(value);
    return Local<Value>(reinterpret_cast<Value*>(result));
  } else {
    return Local<Value>();
  }
}


void* External::Unwrap(Handle<v8::Value> obj) {
#ifdef V8_ENABLE_CHECKS
  return FullUnwrap(obj);
#else
  return QuickUnwrap(obj);
#endif
}


void* External::QuickUnwrap(Handle<v8::Value> wrapper) {
  typedef internal::Object O;
  O* obj = *reinterpret_cast<O**>(const_cast<v8::Value*>(*wrapper));
  return internal::Internals::GetExternalPointer(obj);
}


void* Object::GetPointerFromInternalField(int index) {
  typedef internal::Object O;
  typedef internal::Internals I;

  O* obj = *reinterpret_cast<O**>(this);

  if (I::GetInstanceType(obj) == I::kJSObjectType) {
    // If the object is a plain JSObject, which is the common case,
    // we know where to find the internal fields and can return the
    // value directly.
    int offset = I::kJSObjectHeaderSize + (sizeof(void*) * index);
    O* value = I::ReadField<O*>(obj, offset);
    return I::GetExternalPointer(value);
  }

  return SlowGetPointerFromInternalField(index);
}


String* String::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<String*>(value);
}


String::ExternalStringResource* String::GetExternalStringResource() const {
  typedef internal::Object O;
  typedef internal::Internals I;
  O* obj = *reinterpret_cast<O**>(const_cast<String*>(this));
  String::ExternalStringResource* result;
  if (I::IsExternalTwoByteString(I::GetInstanceType(obj))) {
    void* value = I::ReadField<void*>(obj, I::kStringResourceOffset);
    result = reinterpret_cast<String::ExternalStringResource*>(value);
  } else {
    result = NULL;
  }
#ifdef V8_ENABLE_CHECKS
  VerifyExternalStringResource(result);
#endif
  return result;
}


bool Value::IsString() const {
#ifdef V8_ENABLE_CHECKS
  return FullIsString();
#else
  return QuickIsString();
#endif
}

bool Value::QuickIsString() const {
  typedef internal::Object O;
  typedef internal::Internals I;
  O* obj = *reinterpret_cast<O**>(const_cast<Value*>(this));
  if (!I::HasHeapObjectTag(obj)) return false;
  return (I::GetInstanceType(obj) < I::kFirstNonstringType);
}


Number* Number::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Number*>(value);
}


Integer* Integer::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Integer*>(value);
}


Date* Date::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Date*>(value);
}


Object* Object::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Object*>(value);
}


Array* Array::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Array*>(value);
}


Function* Function::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<Function*>(value);
}


External* External::Cast(v8::Value* value) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(value);
#endif
  return static_cast<External*>(value);
}


Local<Value> AccessorInfo::Data() const {
  return Local<Value>(reinterpret_cast<Value*>(&args_[-2]));
}


Local<Object> AccessorInfo::This() const {
  return Local<Object>(reinterpret_cast<Object*>(&args_[0]));
}


Local<Object> AccessorInfo::Holder() const {
  return Local<Object>(reinterpret_cast<Object*>(&args_[-1]));
}


/**
 * \example shell.cc
 * A simple shell that takes a list of expressions on the
 * command-line and executes them.
 */


/**
 * \example process.cc
 */


}  // namespace v8


#undef V8EXPORT
#undef TYPE_CHECK


#endif  // V8_H_
