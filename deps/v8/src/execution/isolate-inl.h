// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_ISOLATE_INL_H_
#define V8_EXECUTION_ISOLATE_INL_H_

#include "src/execution/isolate.h"
#include "src/objects/cell-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/oddball.h"
#include "src/objects/property-cell.h"
#include "src/objects/regexp-match-info.h"
#include "src/objects/shared-function-info.h"

namespace v8 {
namespace internal {

IsolateAllocationMode Isolate::isolate_allocation_mode() {
  return isolate_allocator_->mode();
}

void Isolate::set_context(Context context) {
  DCHECK(context.is_null() || context.IsContext());
  thread_local_top()->context_ = context;
}

Handle<NativeContext> Isolate::native_context() {
  return handle(context().native_context(), this);
}

NativeContext Isolate::raw_native_context() {
  return context().native_context();
}

Object Isolate::pending_exception() {
  DCHECK(has_pending_exception());
  DCHECK(!thread_local_top()->pending_exception_.IsException(this));
  return thread_local_top()->pending_exception_;
}

void Isolate::set_pending_exception(Object exception_obj) {
  DCHECK(!exception_obj.IsException(this));
  thread_local_top()->pending_exception_ = exception_obj;
}

void Isolate::clear_pending_exception() {
  DCHECK(!thread_local_top()->pending_exception_.IsException(this));
  thread_local_top()->pending_exception_ = ReadOnlyRoots(this).the_hole_value();
}

bool Isolate::has_pending_exception() {
  DCHECK(!thread_local_top()->pending_exception_.IsException(this));
  return !thread_local_top()->pending_exception_.IsTheHole(this);
}

void Isolate::clear_pending_message() {
  thread_local_top()->pending_message_obj_ =
      ReadOnlyRoots(this).the_hole_value();
}

Object Isolate::scheduled_exception() {
  DCHECK(has_scheduled_exception());
  DCHECK(!thread_local_top()->scheduled_exception_.IsException(this));
  return thread_local_top()->scheduled_exception_;
}

bool Isolate::has_scheduled_exception() {
  DCHECK(!thread_local_top()->scheduled_exception_.IsException(this));
  return thread_local_top()->scheduled_exception_ !=
         ReadOnlyRoots(this).the_hole_value();
}

void Isolate::clear_scheduled_exception() {
  DCHECK(!thread_local_top()->scheduled_exception_.IsException(this));
  thread_local_top()->scheduled_exception_ =
      ReadOnlyRoots(this).the_hole_value();
}

bool Isolate::is_catchable_by_javascript(Object exception) {
  return exception != ReadOnlyRoots(heap()).termination_exception();
}

void Isolate::FireBeforeCallEnteredCallback() {
  for (auto& callback : before_call_entered_callbacks_) {
    callback(reinterpret_cast<v8::Isolate*>(this));
  }
}

Handle<JSGlobalObject> Isolate::global_object() {
  return handle(context().global_object(), this);
}

Handle<JSGlobalProxy> Isolate::global_proxy() {
  return handle(context().global_proxy(), this);
}

Isolate::ExceptionScope::ExceptionScope(Isolate* isolate)
    : isolate_(isolate),
      pending_exception_(isolate_->pending_exception(), isolate_) {}

Isolate::ExceptionScope::~ExceptionScope() {
  isolate_->set_pending_exception(*pending_exception_);
}

#define NATIVE_CONTEXT_FIELD_ACCESSOR(index, type, name)    \
  Handle<type> Isolate::name() {                            \
    return Handle<type>(raw_native_context().name(), this); \
  }                                                         \
  bool Isolate::is_##name(type value) {                     \
    return raw_native_context().is_##name(value);           \
  }
NATIVE_CONTEXT_FIELDS(NATIVE_CONTEXT_FIELD_ACCESSOR)
#undef NATIVE_CONTEXT_FIELD_ACCESSOR

bool Isolate::IsArrayConstructorIntact() {
  Cell array_constructor_cell =
      Cell::cast(root(RootIndex::kArrayConstructorProtector));
  return array_constructor_cell.value() == Smi::FromInt(kProtectorValid);
}

bool Isolate::IsArraySpeciesLookupChainIntact() {
  // Note: It would be nice to have debug checks to make sure that the
  // species protector is accurate, but this would be hard to do for most of
  // what the protector stands for:
  // - You'd need to traverse the heap to check that no Array instance has
  //   a constructor property
  // - To check that Array[Symbol.species] == Array, JS code has to execute,
  //   but JS cannot be invoked in callstack overflow situations
  // All that could be checked reliably is that
  // Array.prototype.constructor == Array. Given that limitation, no check is
  // done here. In place, there are mjsunit tests harmony/array-species* which
  // ensure that behavior is correct in various invalid protector cases.

  PropertyCell species_cell =
      PropertyCell::cast(root(RootIndex::kArraySpeciesProtector));
  return species_cell.value().IsSmi() &&
         Smi::ToInt(species_cell.value()) == kProtectorValid;
}

bool Isolate::IsTypedArraySpeciesLookupChainIntact() {
  PropertyCell species_cell =
      PropertyCell::cast(root(RootIndex::kTypedArraySpeciesProtector));
  return species_cell.value().IsSmi() &&
         Smi::ToInt(species_cell.value()) == kProtectorValid;
}

bool Isolate::IsRegExpSpeciesLookupChainIntact(
    Handle<NativeContext> native_context) {
  DCHECK_EQ(*native_context, this->raw_native_context());
  PropertyCell species_cell = native_context->regexp_species_protector();
  return species_cell.value().IsSmi() &&
         Smi::ToInt(species_cell.value()) == kProtectorValid;
}

bool Isolate::IsPromiseSpeciesLookupChainIntact() {
  PropertyCell species_cell =
      PropertyCell::cast(root(RootIndex::kPromiseSpeciesProtector));
  return species_cell.value().IsSmi() &&
         Smi::ToInt(species_cell.value()) == kProtectorValid;
}

bool Isolate::IsStringLengthOverflowIntact() {
  Cell string_length_cell = Cell::cast(root(RootIndex::kStringLengthProtector));
  return string_length_cell.value() == Smi::FromInt(kProtectorValid);
}

bool Isolate::IsArrayBufferDetachingIntact() {
  PropertyCell buffer_detaching =
      PropertyCell::cast(root(RootIndex::kArrayBufferDetachingProtector));
  return buffer_detaching.value() == Smi::FromInt(kProtectorValid);
}

bool Isolate::IsArrayIteratorLookupChainIntact() {
  PropertyCell array_iterator_cell =
      PropertyCell::cast(root(RootIndex::kArrayIteratorProtector));
  return array_iterator_cell.value() == Smi::FromInt(kProtectorValid);
}

bool Isolate::IsMapIteratorLookupChainIntact() {
  PropertyCell map_iterator_cell =
      PropertyCell::cast(root(RootIndex::kMapIteratorProtector));
  return map_iterator_cell.value() == Smi::FromInt(kProtectorValid);
}

bool Isolate::IsSetIteratorLookupChainIntact() {
  PropertyCell set_iterator_cell =
      PropertyCell::cast(root(RootIndex::kSetIteratorProtector));
  return set_iterator_cell.value() == Smi::FromInt(kProtectorValid);
}

bool Isolate::IsStringIteratorLookupChainIntact() {
  PropertyCell string_iterator_cell =
      PropertyCell::cast(root(RootIndex::kStringIteratorProtector));
  return string_iterator_cell.value() == Smi::FromInt(kProtectorValid);
}

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_ISOLATE_INL_H_
