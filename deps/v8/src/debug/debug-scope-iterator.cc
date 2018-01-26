// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug-scope-iterator.h"

#include "src/api.h"
#include "src/debug/debug.h"
#include "src/debug/liveedit.h"
#include "src/frames-inl.h"
#include "src/isolate.h"
#include "src/wasm/wasm-objects-inl.h"

namespace v8 {

std::unique_ptr<debug::ScopeIterator> debug::ScopeIterator::CreateForFunction(
    v8::Isolate* v8_isolate, v8::Local<v8::Function> v8_func) {
  internal::Handle<internal::JSReceiver> receiver =
      internal::Handle<internal::JSReceiver>::cast(Utils::OpenHandle(*v8_func));

  // Besides JSFunction and JSBoundFunction, {v8_func} could be an
  // ObjectTemplate with a CallAsFunctionHandler. We only handle plain
  // JSFunctions.
  if (!receiver->IsJSFunction()) return nullptr;

  internal::Handle<internal::JSFunction> function =
      internal::Handle<internal::JSFunction>::cast(receiver);

  // Blink has function objects with callable map, JS_SPECIAL_API_OBJECT_TYPE
  // but without context on heap.
  if (!function->has_context()) return nullptr;
  return std::unique_ptr<debug::ScopeIterator>(new internal::DebugScopeIterator(
      reinterpret_cast<internal::Isolate*>(v8_isolate), function));
}

std::unique_ptr<debug::ScopeIterator>
debug::ScopeIterator::CreateForGeneratorObject(
    v8::Isolate* v8_isolate, v8::Local<v8::Object> v8_generator) {
  internal::Handle<internal::Object> generator =
      Utils::OpenHandle(*v8_generator);
  DCHECK(generator->IsJSGeneratorObject());
  return std::unique_ptr<debug::ScopeIterator>(new internal::DebugScopeIterator(
      reinterpret_cast<internal::Isolate*>(v8_isolate),
      internal::Handle<internal::JSGeneratorObject>::cast(generator)));
}

namespace internal {

DebugScopeIterator::DebugScopeIterator(Isolate* isolate,
                                       FrameInspector* frame_inspector)
    : iterator_(isolate, frame_inspector) {
  if (!Done() && ShouldIgnore()) Advance();
}

DebugScopeIterator::DebugScopeIterator(Isolate* isolate,
                                       Handle<JSFunction> function)
    : iterator_(isolate, function) {
  if (!Done() && ShouldIgnore()) Advance();
}

DebugScopeIterator::DebugScopeIterator(Isolate* isolate,
                                       Handle<JSGeneratorObject> generator)
    : iterator_(isolate, generator) {
  if (!Done() && ShouldIgnore()) Advance();
}

bool DebugScopeIterator::Done() { return iterator_.Done(); }

void DebugScopeIterator::Advance() {
  DCHECK(!Done());
  iterator_.Next();
  while (!Done() && ShouldIgnore()) {
    iterator_.Next();
  }
}

bool DebugScopeIterator::ShouldIgnore() {
  // Almost always Script scope will be empty, so just filter out that noise.
  // Also drop empty Block, Eval and Script scopes, should we get any.
  DCHECK(!Done());
  debug::ScopeIterator::ScopeType type = GetType();
  if (type != debug::ScopeIterator::ScopeTypeBlock &&
      type != debug::ScopeIterator::ScopeTypeScript &&
      type != debug::ScopeIterator::ScopeTypeEval &&
      type != debug::ScopeIterator::ScopeTypeModule) {
    return false;
  }

  // TODO(kozyatinskiy): make this function faster.
  Handle<JSObject> value;
  if (!iterator_.ScopeObject().ToHandle(&value)) return false;
  Handle<FixedArray> keys =
      KeyAccumulator::GetKeys(value, KeyCollectionMode::kOwnOnly,
                              ENUMERABLE_STRINGS,
                              GetKeysConversion::kConvertToString)
          .ToHandleChecked();
  return keys->length() == 0;
}

v8::debug::ScopeIterator::ScopeType DebugScopeIterator::GetType() {
  DCHECK(!Done());
  return static_cast<v8::debug::ScopeIterator::ScopeType>(iterator_.Type());
}

v8::Local<v8::Object> DebugScopeIterator::GetObject() {
  DCHECK(!Done());
  Handle<JSObject> value;
  if (iterator_.ScopeObject().ToHandle(&value)) {
    return Utils::ToLocal(value);
  }
  return v8::Local<v8::Object>();
}

v8::Local<v8::Function> DebugScopeIterator::GetFunction() {
  DCHECK(!Done());
  Handle<JSFunction> closure = iterator_.GetClosure();
  if (closure.is_null()) return v8::Local<v8::Function>();
  return Utils::ToLocal(closure);
}

debug::Location DebugScopeIterator::GetStartLocation() {
  DCHECK(!Done());
  Handle<JSFunction> closure = iterator_.GetClosure();
  if (closure.is_null()) return debug::Location();
  Object* obj = closure->shared()->script();
  if (!obj->IsScript()) return debug::Location();
  return ToApiHandle<v8::debug::Script>(handle(Script::cast(obj)))
      ->GetSourceLocation(iterator_.start_position());
}

debug::Location DebugScopeIterator::GetEndLocation() {
  DCHECK(!Done());
  Handle<JSFunction> closure = iterator_.GetClosure();
  if (closure.is_null()) return debug::Location();
  Object* obj = closure->shared()->script();
  if (!obj->IsScript()) return debug::Location();
  return ToApiHandle<v8::debug::Script>(handle(Script::cast(obj)))
      ->GetSourceLocation(iterator_.end_position());
}

bool DebugScopeIterator::SetVariableValue(v8::Local<v8::String> name,
                                          v8::Local<v8::Value> value) {
  DCHECK(!Done());
  return iterator_.SetVariableValue(Utils::OpenHandle(*name),
                                    Utils::OpenHandle(*value));
}

DebugWasmScopeIterator::DebugWasmScopeIterator(Isolate* isolate,
                                               StandardFrame* frame,
                                               int inlined_frame_index)
    : isolate_(isolate),
      frame_(frame),
      inlined_frame_index_(inlined_frame_index),
      type_(debug::ScopeIterator::ScopeTypeGlobal) {}

bool DebugWasmScopeIterator::Done() {
  return type_ != debug::ScopeIterator::ScopeTypeGlobal &&
         type_ != debug::ScopeIterator::ScopeTypeLocal;
}

void DebugWasmScopeIterator::Advance() {
  DCHECK(!Done());
  if (type_ == debug::ScopeIterator::ScopeTypeGlobal) {
    type_ = debug::ScopeIterator::ScopeTypeLocal;
  } else {
    // We use ScopeTypeWith type as marker for done.
    type_ = debug::ScopeIterator::ScopeTypeWith;
  }
}

v8::debug::ScopeIterator::ScopeType DebugWasmScopeIterator::GetType() {
  DCHECK(!Done());
  return type_;
}

v8::Local<v8::Object> DebugWasmScopeIterator::GetObject() {
  DCHECK(!Done());
  Handle<WasmDebugInfo> debug_info(
      WasmInterpreterEntryFrame::cast(frame_)->debug_info(), isolate_);
  switch (type_) {
    case debug::ScopeIterator::ScopeTypeGlobal:
      return Utils::ToLocal(WasmDebugInfo::GetGlobalScopeObject(
          debug_info, frame_->fp(), inlined_frame_index_));
    case debug::ScopeIterator::ScopeTypeLocal:
      return Utils::ToLocal(WasmDebugInfo::GetLocalScopeObject(
          debug_info, frame_->fp(), inlined_frame_index_));
    default:
      return v8::Local<v8::Object>();
  }
  return v8::Local<v8::Object>();
}

v8::Local<v8::Function> DebugWasmScopeIterator::GetFunction() {
  DCHECK(!Done());
  return v8::Local<v8::Function>();
}

debug::Location DebugWasmScopeIterator::GetStartLocation() {
  DCHECK(!Done());
  return debug::Location();
}

debug::Location DebugWasmScopeIterator::GetEndLocation() {
  DCHECK(!Done());
  return debug::Location();
}

bool DebugWasmScopeIterator::SetVariableValue(v8::Local<v8::String> name,
                                              v8::Local<v8::Value> value) {
  DCHECK(!Done());
  return false;
}
}  // namespace internal
}  // namespace v8
