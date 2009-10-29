// Copyright 2008 the V8 project authors. All rights reserved.
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

#ifndef V8_API_H_
#define V8_API_H_

#include "apiutils.h"
#include "factory.h"

namespace v8 {

// Constants used in the implementation of the API.  The most natural thing
// would usually be to place these with the classes that use them, but
// we want to keep them out of v8.h because it is an externally
// visible file.
class Consts {
 public:
  enum TemplateType {
    FUNCTION_TEMPLATE = 0,
    OBJECT_TEMPLATE = 1
  };
};


// Utilities for working with neander-objects, primitive
// env-independent JSObjects used by the api.
class NeanderObject {
 public:
  explicit NeanderObject(int size);
  inline NeanderObject(v8::internal::Handle<v8::internal::Object> obj);
  inline NeanderObject(v8::internal::Object* obj);
  inline v8::internal::Object* get(int index);
  inline void set(int index, v8::internal::Object* value);
  inline v8::internal::Handle<v8::internal::JSObject> value() { return value_; }
  int size();
 private:
  v8::internal::Handle<v8::internal::JSObject> value_;
};


// Utilities for working with neander-arrays, a simple extensible
// array abstraction built on neander-objects.
class NeanderArray {
 public:
  NeanderArray();
  inline NeanderArray(v8::internal::Handle<v8::internal::Object> obj);
  inline v8::internal::Handle<v8::internal::JSObject> value() {
    return obj_.value();
  }

  void add(v8::internal::Handle<v8::internal::Object> value);

  int length();

  v8::internal::Object* get(int index);
  // Change the value at an index to undefined value. If the index is
  // out of bounds, the request is ignored. Returns the old value.
  void set(int index, v8::internal::Object* value);
 private:
  NeanderObject obj_;
};


NeanderObject::NeanderObject(v8::internal::Handle<v8::internal::Object> obj)
    : value_(v8::internal::Handle<v8::internal::JSObject>::cast(obj)) { }


NeanderObject::NeanderObject(v8::internal::Object* obj)
    : value_(v8::internal::Handle<v8::internal::JSObject>(
        v8::internal::JSObject::cast(obj))) { }


NeanderArray::NeanderArray(v8::internal::Handle<v8::internal::Object> obj)
    : obj_(obj) { }


v8::internal::Object* NeanderObject::get(int offset) {
  ASSERT(value()->HasFastElements());
  return v8::internal::FixedArray::cast(value()->elements())->get(offset);
}


void NeanderObject::set(int offset, v8::internal::Object* value) {
  ASSERT(value_->HasFastElements());
  v8::internal::FixedArray::cast(value_->elements())->set(offset, value);
}


template <typename T> static inline T ToCData(v8::internal::Object* obj) {
  STATIC_ASSERT(sizeof(T) == sizeof(v8::internal::Address));
  return reinterpret_cast<T>(
      reinterpret_cast<intptr_t>(v8::internal::Proxy::cast(obj)->proxy()));
}


template <typename T>
static inline v8::internal::Handle<v8::internal::Object> FromCData(T obj) {
  STATIC_ASSERT(sizeof(T) == sizeof(v8::internal::Address));
  return v8::internal::Factory::NewProxy(
      reinterpret_cast<v8::internal::Address>(reinterpret_cast<intptr_t>(obj)));
}


v8::Arguments::Arguments(v8::Local<v8::Value> data,
                         v8::Local<v8::Object> holder,
                         v8::Local<v8::Function> callee,
                         bool is_construct_call,
                         void** values, int length)
    : data_(data), holder_(holder), callee_(callee),
      is_construct_call_(is_construct_call),
      values_(values), length_(length) { }


enum ExtensionTraversalState {
  UNVISITED, VISITED, INSTALLED
};


class RegisteredExtension {
 public:
  explicit RegisteredExtension(Extension* extension);
  static void Register(RegisteredExtension* that);
  Extension* extension() { return extension_; }
  RegisteredExtension* next() { return next_; }
  RegisteredExtension* next_auto() { return next_auto_; }
  ExtensionTraversalState state() { return state_; }
  void set_state(ExtensionTraversalState value) { state_ = value; }
  static RegisteredExtension* first_extension() { return first_extension_; }
 private:
  Extension* extension_;
  RegisteredExtension* next_;
  RegisteredExtension* next_auto_;
  ExtensionTraversalState state_;
  static RegisteredExtension* first_extension_;
  static RegisteredExtension* first_auto_extension_;
};


class Utils {
 public:
  static bool ReportApiFailure(const char* location, const char* message);

  static Local<FunctionTemplate> ToFunctionTemplate(NeanderObject obj);
  static Local<ObjectTemplate> ToObjectTemplate(NeanderObject obj);

  static inline Local<Context> ToLocal(
      v8::internal::Handle<v8::internal::Context> obj);
  static inline Local<Value> ToLocal(
      v8::internal::Handle<v8::internal::Object> obj);
  static inline Local<Function> ToLocal(
      v8::internal::Handle<v8::internal::JSFunction> obj);
  static inline Local<String> ToLocal(
      v8::internal::Handle<v8::internal::String> obj);
  static inline Local<Object> ToLocal(
      v8::internal::Handle<v8::internal::JSObject> obj);
  static inline Local<Array> ToLocal(
      v8::internal::Handle<v8::internal::JSArray> obj);
  static inline Local<External> ToLocal(
      v8::internal::Handle<v8::internal::Proxy> obj);
  static inline Local<Message> MessageToLocal(
      v8::internal::Handle<v8::internal::Object> obj);
  static inline Local<Number> NumberToLocal(
      v8::internal::Handle<v8::internal::Object> obj);
  static inline Local<Integer> IntegerToLocal(
      v8::internal::Handle<v8::internal::Object> obj);
  static inline Local<Uint32> Uint32ToLocal(
      v8::internal::Handle<v8::internal::Object> obj);
  static inline Local<FunctionTemplate> ToLocal(
      v8::internal::Handle<v8::internal::FunctionTemplateInfo> obj);
  static inline Local<ObjectTemplate> ToLocal(
      v8::internal::Handle<v8::internal::ObjectTemplateInfo> obj);
  static inline Local<Signature> ToLocal(
      v8::internal::Handle<v8::internal::SignatureInfo> obj);
  static inline Local<TypeSwitch> ToLocal(
      v8::internal::Handle<v8::internal::TypeSwitchInfo> obj);

  static inline v8::internal::Handle<v8::internal::TemplateInfo>
      OpenHandle(const Template* that);
  static inline v8::internal::Handle<v8::internal::FunctionTemplateInfo>
      OpenHandle(const FunctionTemplate* that);
  static inline v8::internal::Handle<v8::internal::ObjectTemplateInfo>
      OpenHandle(const ObjectTemplate* that);
  static inline v8::internal::Handle<v8::internal::Object>
      OpenHandle(const Data* data);
  static inline v8::internal::Handle<v8::internal::JSObject>
      OpenHandle(const v8::Object* data);
  static inline v8::internal::Handle<v8::internal::JSArray>
      OpenHandle(const v8::Array* data);
  static inline v8::internal::Handle<v8::internal::String>
      OpenHandle(const String* data);
  static inline v8::internal::Handle<v8::internal::JSFunction>
      OpenHandle(const Script* data);
  static inline v8::internal::Handle<v8::internal::JSFunction>
      OpenHandle(const Function* data);
  static inline v8::internal::Handle<v8::internal::JSObject>
      OpenHandle(const Message* message);
  static inline v8::internal::Handle<v8::internal::Context>
      OpenHandle(const v8::Context* context);
  static inline v8::internal::Handle<v8::internal::SignatureInfo>
      OpenHandle(const v8::Signature* sig);
  static inline v8::internal::Handle<v8::internal::TypeSwitchInfo>
      OpenHandle(const v8::TypeSwitch* that);
  static inline v8::internal::Handle<v8::internal::Proxy>
      OpenHandle(const v8::External* that);
};


template <class T>
static inline T* ToApi(v8::internal::Handle<v8::internal::Object> obj) {
  return reinterpret_cast<T*>(obj.location());
}


template <class T>
v8::internal::Handle<T> v8::internal::Handle<T>::EscapeFrom(
    v8::HandleScope* scope) {
  return Utils::OpenHandle(*scope->Close(Utils::ToLocal(*this)));
}


// Implementations of ToLocal

#define MAKE_TO_LOCAL(Name, From, To)                                       \
  Local<v8::To> Utils::Name(v8::internal::Handle<v8::internal::From> obj) { \
    ASSERT(!obj->IsTheHole());                                              \
    return Local<To>(reinterpret_cast<To*>(obj.location()));                \
  }

MAKE_TO_LOCAL(ToLocal, Context, Context)
MAKE_TO_LOCAL(ToLocal, Object, Value)
MAKE_TO_LOCAL(ToLocal, JSFunction, Function)
MAKE_TO_LOCAL(ToLocal, String, String)
MAKE_TO_LOCAL(ToLocal, JSObject, Object)
MAKE_TO_LOCAL(ToLocal, JSArray, Array)
MAKE_TO_LOCAL(ToLocal, Proxy, External)
MAKE_TO_LOCAL(ToLocal, FunctionTemplateInfo, FunctionTemplate)
MAKE_TO_LOCAL(ToLocal, ObjectTemplateInfo, ObjectTemplate)
MAKE_TO_LOCAL(ToLocal, SignatureInfo, Signature)
MAKE_TO_LOCAL(ToLocal, TypeSwitchInfo, TypeSwitch)
MAKE_TO_LOCAL(MessageToLocal, Object, Message)
MAKE_TO_LOCAL(NumberToLocal, Object, Number)
MAKE_TO_LOCAL(IntegerToLocal, Object, Integer)
MAKE_TO_LOCAL(Uint32ToLocal, Object, Uint32)

#undef MAKE_TO_LOCAL


// Implementations of OpenHandle

#define MAKE_OPEN_HANDLE(From, To) \
  v8::internal::Handle<v8::internal::To> Utils::OpenHandle(\
    const v8::From* that) { \
    return v8::internal::Handle<v8::internal::To>( \
        reinterpret_cast<v8::internal::To**>(const_cast<v8::From*>(that))); \
  }

MAKE_OPEN_HANDLE(Template, TemplateInfo)
MAKE_OPEN_HANDLE(FunctionTemplate, FunctionTemplateInfo)
MAKE_OPEN_HANDLE(ObjectTemplate, ObjectTemplateInfo)
MAKE_OPEN_HANDLE(Signature, SignatureInfo)
MAKE_OPEN_HANDLE(TypeSwitch, TypeSwitchInfo)
MAKE_OPEN_HANDLE(Data, Object)
MAKE_OPEN_HANDLE(Object, JSObject)
MAKE_OPEN_HANDLE(Array, JSArray)
MAKE_OPEN_HANDLE(String, String)
MAKE_OPEN_HANDLE(Script, JSFunction)
MAKE_OPEN_HANDLE(Function, JSFunction)
MAKE_OPEN_HANDLE(Message, JSObject)
MAKE_OPEN_HANDLE(Context, Context)
MAKE_OPEN_HANDLE(External, Proxy)

#undef MAKE_OPEN_HANDLE


namespace internal {

// This class is here in order to be able to declare it a friend of
// HandleScope.  Moving these methods to be members of HandleScope would be
// neat in some ways, but it would expose external implementation details in
// our public header file, which is undesirable.
//
// There is a singleton instance of this class to hold the per-thread data.
// For multithreaded V8 programs this data is copied in and out of storage
// so that the currently executing thread always has its own copy of this
// data.
class HandleScopeImplementer {
 public:

  HandleScopeImplementer()
      : blocks_(0),
        entered_contexts_(0),
        saved_contexts_(0),
        spare_(NULL),
        ignore_out_of_memory_(false),
        call_depth_(0) { }

  static HandleScopeImplementer* instance();

  // Threading support for handle data.
  static int ArchiveSpacePerThread();
  static char* RestoreThread(char* from);
  static char* ArchiveThread(char* to);
  static void FreeThreadResources();

  // Garbage collection support.
  static void Iterate(v8::internal::ObjectVisitor* v);
  static char* Iterate(v8::internal::ObjectVisitor* v, char* data);


  inline internal::Object** GetSpareOrNewBlock();
  inline void DeleteExtensions(int extensions);

  inline void IncrementCallDepth() {call_depth_++;}
  inline void DecrementCallDepth() {call_depth_--;}
  inline bool CallDepthIsZero() { return call_depth_ == 0; }

  inline void EnterContext(Handle<Object> context);
  inline bool LeaveLastContext();

  // Returns the last entered context or an empty handle if no
  // contexts have been entered.
  inline Handle<Object> LastEnteredContext();

  inline void SaveContext(Context* context);
  inline Context* RestoreContext();
  inline bool HasSavedContexts();

  inline List<internal::Object**>* blocks() { return &blocks_; }
  inline bool ignore_out_of_memory() { return ignore_out_of_memory_; }
  inline void set_ignore_out_of_memory(bool value) {
    ignore_out_of_memory_ = value;
  }

 private:
  void ResetAfterArchive() {
    blocks_.Initialize(0);
    entered_contexts_.Initialize(0);
    saved_contexts_.Initialize(0);
    spare_ = NULL;
    ignore_out_of_memory_ = false;
    call_depth_ = 0;
  }

  void Free() {
    ASSERT(blocks_.length() == 0);
    ASSERT(entered_contexts_.length() == 0);
    ASSERT(saved_contexts_.length() == 0);
    blocks_.Free();
    entered_contexts_.Free();
    saved_contexts_.Free();
    if (spare_ != NULL) {
      DeleteArray(spare_);
      spare_ = NULL;
    }
    ASSERT(call_depth_ == 0);
  }

  List<internal::Object**> blocks_;
  // Used as a stack to keep track of entered contexts.
  List<Handle<Object> > entered_contexts_;
  // Used as a stack to keep track of saved contexts.
  List<Context*> saved_contexts_;
  Object** spare_;
  bool ignore_out_of_memory_;
  int call_depth_;
  // This is only used for threading support.
  v8::ImplementationUtilities::HandleScopeData handle_scope_data_;

  void IterateThis(ObjectVisitor* v);
  char* RestoreThreadHelper(char* from);
  char* ArchiveThreadHelper(char* to);

  DISALLOW_COPY_AND_ASSIGN(HandleScopeImplementer);
};


static const int kHandleBlockSize = v8::internal::KB - 2;  // fit in one page


void HandleScopeImplementer::SaveContext(Context* context) {
  saved_contexts_.Add(context);
}


Context* HandleScopeImplementer::RestoreContext() {
  return saved_contexts_.RemoveLast();
}


bool HandleScopeImplementer::HasSavedContexts() {
  return !saved_contexts_.is_empty();
}


void HandleScopeImplementer::EnterContext(Handle<Object> context) {
  entered_contexts_.Add(context);
}


bool HandleScopeImplementer::LeaveLastContext() {
  if (entered_contexts_.is_empty()) return false;
  entered_contexts_.RemoveLast();
  return true;
}


Handle<Object> HandleScopeImplementer::LastEnteredContext() {
  if (entered_contexts_.is_empty()) return Handle<Object>::null();
  return entered_contexts_.last();
}


// If there's a spare block, use it for growing the current scope.
internal::Object** HandleScopeImplementer::GetSpareOrNewBlock() {
  internal::Object** block = (spare_ != NULL) ?
      spare_ :
      NewArray<internal::Object*>(kHandleBlockSize);
  spare_ = NULL;
  return block;
}


void HandleScopeImplementer::DeleteExtensions(int extensions) {
  if (spare_ != NULL) {
    DeleteArray(spare_);
    spare_ = NULL;
  }
  for (int i = extensions; i > 1; --i) {
    internal::Object** block = blocks_.RemoveLast();
#ifdef DEBUG
    v8::ImplementationUtilities::ZapHandleRange(block,
                                                &block[kHandleBlockSize]);
#endif
    DeleteArray(block);
  }
  spare_ = blocks_.RemoveLast();
#ifdef DEBUG
  v8::ImplementationUtilities::ZapHandleRange(
      spare_,
      &spare_[kHandleBlockSize]);
#endif
}

} }  // namespace v8::internal

#endif  // V8_API_H_
