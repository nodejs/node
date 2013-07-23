// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "node.h"
#include "node_internals.h"
#include "smalloc.h"

#include "v8.h"
#include "v8-profiler.h"

#include <string.h>
#include <assert.h>

#define ALLOC_ID (0xA10C)

namespace node {
namespace smalloc {

using v8::External;
using v8::FunctionCallbackInfo;
using v8::Handle;
using v8::HandleScope;
using v8::HeapProfiler;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::Persistent;
using v8::RetainedObjectInfo;
using v8::String;
using v8::Uint32;
using v8::Value;
using v8::kExternalUnsignedByteArray;


struct CallbackInfo {
  void* hint;
  FreeCallback cb;
  Persistent<Object> p_obj;
};

void TargetCallback(Isolate* isolate,
                    Persistent<Object>* target,
                    char* arg);
void TargetFreeCallback(Isolate* isolate,
                        Persistent<Object>* target,
                        CallbackInfo* arg);
void TargetFreeCallback(Isolate* isolate,
                        Local<Object> target,
                        CallbackInfo* cb_info);

Cached<String> smalloc_sym;
static bool using_alloc_cb;


// copyOnto(source, source_start, dest, dest_start, copy_length)
void CopyOnto(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Local<Object> source = args[0]->ToObject();
  Local<Object> dest = args[2]->ToObject();

  if (!source->HasIndexedPropertiesInExternalArrayData())
    return ThrowError("source has no external array data");
  if (!dest->HasIndexedPropertiesInExternalArrayData())
    return ThrowError("dest has no external array data");

  size_t source_start = args[1]->Uint32Value();
  size_t dest_start = args[3]->Uint32Value();
  size_t copy_length = args[4]->Uint32Value();
  size_t source_length = source->GetIndexedPropertiesExternalArrayDataLength();
  size_t dest_length = dest->GetIndexedPropertiesExternalArrayDataLength();
  char* source_data = static_cast<char*>(
      source->GetIndexedPropertiesExternalArrayData());
  char* dest_data = static_cast<char*>(
      dest->GetIndexedPropertiesExternalArrayData());

  // necessary to check in case (source|dest)_start _and_ copy_length overflow
  if (copy_length > source_length)
    return ThrowRangeError("copy_length > source_length");
  if (copy_length > dest_length)
    return ThrowRangeError("copy_length > dest_length");
  if (source_start > source_length)
    return ThrowRangeError("source_start > source_length");
  if (dest_start > dest_length)
    return ThrowRangeError("dest_start > dest_length");

  // now we can guarantee these will catch oob access and *_start overflow
  if (source_start + copy_length > source_length)
    return ThrowRangeError("source_start + copy_length > source_length");
  if (dest_start + copy_length > dest_length)
    return ThrowRangeError("dest_start + copy_length > dest_length");

  memmove(dest_data + dest_start, source_data + source_start, copy_length);
}


// for internal use: dest._data = sliceOnto(source, dest, start, end);
void SliceOnto(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  Local<Object> source = args[0]->ToObject();
  Local<Object> dest = args[1]->ToObject();
  char* source_data = static_cast<char*>(
      source->GetIndexedPropertiesExternalArrayData());
  size_t source_len = source->GetIndexedPropertiesExternalArrayDataLength();
  size_t start = args[2]->Uint32Value();
  size_t end = args[3]->Uint32Value();
  size_t length = end - start;

  assert(!dest->HasIndexedPropertiesInExternalArrayData());
  assert(source_data != NULL || length == 0);
  assert(end <= source_len);
  assert(start <= end);

  dest->SetIndexedPropertiesToExternalArrayData(source_data + start,
                                                kExternalUnsignedByteArray,
                                                length);
  args.GetReturnValue().Set(source);
}


// for internal use: alloc(obj, n);
void Alloc(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  if (!args[0]->IsObject())
    return ThrowTypeError("argument must be an Object");

  Local<Object> obj = args[0]->ToObject();
  size_t length = args[1]->Uint32Value();

  if (obj->HasIndexedPropertiesInExternalArrayData())
    return ThrowTypeError("object already has external array data");

  Alloc(obj, length);
  args.GetReturnValue().Set(obj);
}


void Alloc(Handle<Object> obj, size_t length) {
  assert(length <= kMaxLength);

  if (length == 0)
    return Alloc(obj, NULL, length);

  char* data = static_cast<char*>(malloc(length));
  if (data == NULL)
    FatalError("node::smalloc::Alloc(Handle<Object>, size_t)", "Out Of Memory");
  Alloc(obj, data, length);
}


void Alloc(Handle<Object> obj, char* data, size_t length) {
  assert(!obj->HasIndexedPropertiesInExternalArrayData());
  Persistent<Object> p_obj(node_isolate, obj);
  node_isolate->AdjustAmountOfExternalAllocatedMemory(length);
  p_obj.MakeWeak(data, TargetCallback);
  p_obj.MarkIndependent();
  p_obj.SetWrapperClassId(ALLOC_ID);
  obj->SetIndexedPropertiesToExternalArrayData(data,
                                               kExternalUnsignedByteArray,
                                               length);
}


void TargetCallback(Isolate* isolate,
                    Persistent<Object>* target,
                    char* data) {
  HandleScope handle_scope(isolate);
  Local<Object> obj = PersistentToLocal(isolate, *target);
  int len = obj->GetIndexedPropertiesExternalArrayDataLength();
  if (data != NULL && len > 0) {
    isolate->AdjustAmountOfExternalAllocatedMemory(-len);
    free(data);
  }
  (*target).Dispose();
}


// for internal use: dispose(obj);
void AllocDispose(const FunctionCallbackInfo<Value>& args) {
  AllocDispose(args[0]->ToObject());
}


void AllocDispose(Handle<Object> obj) {
  HandleScope scope(node_isolate);

  if (using_alloc_cb && obj->Has(smalloc_sym)) {
    Local<External> ext = obj->GetHiddenValue(smalloc_sym).As<External>();
    CallbackInfo* cb_info = static_cast<CallbackInfo*>(ext->Value());
    Local<Object> obj = PersistentToLocal(cb_info->p_obj);
    TargetFreeCallback(node_isolate, obj, cb_info);
    return;
  }

  char* data = static_cast<char*>(obj->GetIndexedPropertiesExternalArrayData());
  size_t length = obj->GetIndexedPropertiesExternalArrayDataLength();

  if (data != NULL) {
    obj->SetIndexedPropertiesToExternalArrayData(NULL,
                                                 kExternalUnsignedByteArray,
                                                 0);
    free(data);
  }
  if (length != 0) {
    node_isolate->AdjustAmountOfExternalAllocatedMemory(-length);
  }
}


void Alloc(Handle<Object> obj, size_t length, FreeCallback fn, void* hint) {
  assert(length <= kMaxLength);

  char* data = new char[length];
  Alloc(obj, data, length, fn, hint);
}


void Alloc(Handle<Object> obj,
           char* data,
           size_t length,
           FreeCallback fn,
           void* hint) {
  assert(!obj->HasIndexedPropertiesInExternalArrayData());

  if (smalloc_sym.IsEmpty()) {
    smalloc_sym = String::New("_smalloc_p");
    using_alloc_cb = true;
  }

  CallbackInfo* cb_info = new CallbackInfo;
  cb_info->cb = fn;
  cb_info->hint = hint;
  cb_info->p_obj.Reset(node_isolate, obj);
  obj->SetHiddenValue(smalloc_sym, External::New(cb_info));

  node_isolate->AdjustAmountOfExternalAllocatedMemory(length +
                                                      sizeof(*cb_info));
  cb_info->p_obj.MakeWeak(cb_info, TargetFreeCallback);
  cb_info->p_obj.MarkIndependent();
  cb_info->p_obj.SetWrapperClassId(ALLOC_ID);
  obj->SetIndexedPropertiesToExternalArrayData(data,
                                               kExternalUnsignedByteArray,
                                               length);
}


void TargetFreeCallback(Isolate* isolate,
                        Persistent<Object>* target,
                        CallbackInfo* cb_info) {
  HandleScope handle_scope(isolate);
  Local<Object> obj = PersistentToLocal(isolate, *target);
  TargetFreeCallback(isolate, obj, cb_info);
}


void TargetFreeCallback(Isolate* isolate,
                        Local<Object> obj,
                        CallbackInfo* cb_info) {
  HandleScope handle_scope(isolate);
  char* data = static_cast<char*>(obj->GetIndexedPropertiesExternalArrayData());
  int len = obj->GetIndexedPropertiesExternalArrayDataLength();
  isolate->AdjustAmountOfExternalAllocatedMemory(-(len + sizeof(*cb_info)));
  cb_info->p_obj.Dispose();
  cb_info->cb(data, cb_info->hint);
  delete cb_info;
}


class RetainedAllocInfo: public RetainedObjectInfo {
 public:
  explicit RetainedAllocInfo(Handle<Value> wrapper);

  virtual void Dispose();
  virtual bool IsEquivalent(RetainedObjectInfo* other);
  virtual intptr_t GetHash();
  virtual const char* GetLabel();
  virtual intptr_t GetSizeInBytes();

 private:
  static const char label_[];
  char* data_;
  int length_;
};


const char RetainedAllocInfo::label_[] = "smalloc";


RetainedAllocInfo::RetainedAllocInfo(Handle<Value> wrapper) {
  Local<Object> obj = wrapper->ToObject();
  length_ = obj->GetIndexedPropertiesExternalArrayDataLength();
  data_ = static_cast<char*>(obj->GetIndexedPropertiesExternalArrayData());
}


void RetainedAllocInfo::Dispose() {
  delete this;
}


bool RetainedAllocInfo::IsEquivalent(RetainedObjectInfo* other) {
  return label_ == other->GetLabel() &&
         data_ == static_cast<RetainedAllocInfo*>(other)->data_;
}


intptr_t RetainedAllocInfo::GetHash() {
  return reinterpret_cast<intptr_t>(data_);
}


const char* RetainedAllocInfo::GetLabel() {
  return label_;
}


intptr_t RetainedAllocInfo::GetSizeInBytes() {
  return length_;
}


RetainedObjectInfo* WrapperInfo(uint16_t class_id, Handle<Value> wrapper) {
  return new RetainedAllocInfo(wrapper);
}


void Initialize(Handle<Object> exports) {
  NODE_SET_METHOD(exports, "copyOnto", CopyOnto);
  NODE_SET_METHOD(exports, "sliceOnto", SliceOnto);

  NODE_SET_METHOD(exports, "alloc", Alloc);
  NODE_SET_METHOD(exports, "dispose", AllocDispose);

  exports->Set(String::New("kMaxLength"),
               Uint32::New(kMaxLength, node_isolate));

  // for performance, begin checking if allocation object may contain
  // callbacks if at least one has been set.
  using_alloc_cb = false;

  HeapProfiler* heap_profiler = node_isolate->GetHeapProfiler();
  heap_profiler->SetWrapperClassInfoProvider(ALLOC_ID, WrapperInfo);
}


}  // namespace smalloc
}  // namespace node

NODE_MODULE(node_smalloc, node::smalloc::Initialize)
