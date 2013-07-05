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
#include "smalloc.h"

#include "v8.h"
#include "v8-profiler.h"

#include <string.h>
#include <assert.h>

#define ALLOC_ID (0xA10C)

namespace node {
namespace smalloc {

using v8::Arguments;
using v8::FunctionTemplate;
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
};

typedef v8::WeakReferenceCallbacks<Object, char>::Revivable Callback;
typedef v8::WeakReferenceCallbacks<Object, void>::Revivable CallbackFree;

Callback target_cb;
CallbackFree target_free_cb;

void TargetCallback(Isolate* isolate, Persistent<Object>* target, char* arg);
void TargetFreeCallback(Isolate* isolate,
                        Persistent<Object>* target,
                        void* arg);


// for internal use: copyOnto(source, source_start, dest, dest_start, length)
Handle<Value> CopyOnto(const Arguments& args) {
  HandleScope scope(node_isolate);

  Local<Object> source = args[0]->ToObject();
  Local<Object> dest = args[2]->ToObject();
  size_t source_start = args[1]->Uint32Value();
  size_t dest_start = args[3]->Uint32Value();
  size_t length = args[4]->Uint32Value();
  size_t source_length = source->GetIndexedPropertiesExternalArrayDataLength();
  size_t dest_length = dest->GetIndexedPropertiesExternalArrayDataLength();
  char* source_data = static_cast<char*>(
                              source->GetIndexedPropertiesExternalArrayData());
  char* dest_data = static_cast<char*>(
                                dest->GetIndexedPropertiesExternalArrayData());

  assert(source_data != NULL);
  assert(dest_data != NULL);
  // necessary to check in case (source|dest)_start _and_ length overflow
  assert(length <= source_length);
  assert(length <= dest_length);
  assert(source_start <= source_length);
  assert(dest_start <= dest_length);
  // now we can guarantee these will catch oob access and *_start overflow
  assert(source_start + length <= source_length);
  assert(dest_start + length <= dest_length);

  memmove(dest_data + dest_start, source_data + source_start, length);

  return Undefined(node_isolate);
}


// for internal use: dest._data = sliceOnto(source, dest, start, end);
Handle<Value> SliceOnto(const Arguments& args) {
  HandleScope scope(node_isolate);

  Local<Object> source = args[0]->ToObject();
  Local<Object> dest = args[1]->ToObject();
  char* source_data = static_cast<char*>(source->
                                  GetIndexedPropertiesExternalArrayData());
  size_t source_len = source->GetIndexedPropertiesExternalArrayDataLength();
  size_t start = args[2]->Uint32Value();
  size_t end = args[3]->Uint32Value();

  assert(!dest->HasIndexedPropertiesInExternalArrayData());
  assert(source_data != NULL);
  assert(end <= source_len);
  assert(start <= end);

  dest->SetIndexedPropertiesToExternalArrayData(source_data + start,
                                                kExternalUnsignedByteArray,
                                                end - start);

  return scope.Close(source);
}


// for internal use: alloc(obj, n);
Handle<Value> Alloc(const Arguments& args) {
  HandleScope scope(node_isolate);

  Local<Object> obj = args[0]->ToObject();
  size_t length = args[1]->Uint32Value();

  Alloc(obj, length);

  return scope.Close(obj);
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
  Persistent<Object> p_obj(node_isolate, obj);

  node_isolate->AdjustAmountOfExternalAllocatedMemory(length);
  p_obj.MakeWeak(node_isolate, data, target_cb);
  p_obj.MarkIndependent(node_isolate);
  p_obj.SetWrapperClassId(node_isolate, ALLOC_ID);
  p_obj->SetIndexedPropertiesToExternalArrayData(data,
                                                 kExternalUnsignedByteArray,
                                                 length);
}


void TargetCallback(Isolate* isolate, Persistent<Object>* target, char* data) {
  int len = (*target)->GetIndexedPropertiesExternalArrayDataLength();
  if (data != NULL && len > 0) {
    isolate->AdjustAmountOfExternalAllocatedMemory(-len);
    free(data);
  }
  (*target).Dispose();
  (*target).Clear();
}


Handle<Value> AllocDispose(const Arguments& args) {
  AllocDispose(args[0]->ToObject());
  return Undefined(node_isolate);
}


void AllocDispose(Handle<Object> obj) {
  char* data = static_cast<char*>(obj->GetIndexedPropertiesExternalArrayData());
  size_t length = obj->GetIndexedPropertiesExternalArrayDataLength();

  if (data == NULL || length == 0)
    return;

  obj->SetIndexedPropertiesToExternalArrayData(NULL,
                                               kExternalUnsignedByteArray,
                                               0);
  free(data);
  node_isolate->AdjustAmountOfExternalAllocatedMemory(-length);
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
  assert(data != NULL);

  CallbackInfo* cb_info = new CallbackInfo;
  cb_info->cb = fn;
  cb_info->hint = hint;
  Persistent<Object> p_obj(node_isolate, obj);

  node_isolate->AdjustAmountOfExternalAllocatedMemory(length +
                                                      sizeof(*cb_info));
  p_obj.MakeWeak(node_isolate, static_cast<void*>(cb_info), target_free_cb);
  p_obj.MarkIndependent(node_isolate);
  p_obj.SetWrapperClassId(node_isolate, ALLOC_ID);
  p_obj->SetIndexedPropertiesToExternalArrayData(data,
                                                 kExternalUnsignedByteArray,
                                                 length);
}


// TODO(trevnorris): running AllocDispose will cause data == NULL, which is
// then passed to cb_info->cb, which the user will need to check for.
void TargetFreeCallback(Isolate* isolate,
                        Persistent<Object>* target,
                        void* arg) {
  Local<Object> obj = **target;
  int len = obj->GetIndexedPropertiesExternalArrayDataLength();
  char* data = static_cast<char*>(obj->GetIndexedPropertiesExternalArrayData());
  CallbackInfo* cb_info = static_cast<CallbackInfo*>(arg);
  isolate->AdjustAmountOfExternalAllocatedMemory(-(len + sizeof(*cb_info)));
  (*target).Dispose();
  (*target).Clear();
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
  static const char* label_;
  char* data_;
  int length_;
};


const char* RetainedAllocInfo::label_ = "smalloc";


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

  target_cb = TargetCallback;
  target_free_cb = TargetFreeCallback;

  HeapProfiler* heap_profiler = node_isolate->GetHeapProfiler();
  heap_profiler->SetWrapperClassInfoProvider(ALLOC_ID, WrapperInfo);
}


}  // namespace smalloc
}  // namespace node

NODE_MODULE(node_smalloc, node::smalloc::Initialize)
