// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_ITERATOR_HELPERS_INL_H_
#define V8_OBJECTS_JS_ITERATOR_HELPERS_INL_H_

#include "src/objects/js-iterator-helpers.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/js-objects-inl.h"
#include "src/objects/smi-inl.h"
#include "src/objects/tagged-field-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

JSIteratorHelperState JSIteratorHelper::state() const {
  return static_cast<JSIteratorHelperState>(state_.load().value());
}

void JSIteratorHelper::set_state(JSIteratorHelperState value) {
  state_.store(this, Smi::From31BitPattern(static_cast<int>(value)));
}

Tagged<JSReceiver> JSIteratorHelperSimple::underlying_iterator_object() const {
  return underlying_iterator_object_.load();
}

void JSIteratorHelperSimple::set_underlying_iterator_object(
    Tagged<JSReceiver> value, WriteBarrierMode mode) {
  underlying_iterator_object_.store(this, value, mode);
}

Tagged<JSAny> JSIteratorHelperSimple::underlying_iterator_next() const {
  return underlying_iterator_next_.load();
}

void JSIteratorHelperSimple::set_underlying_iterator_next(
    Tagged<JSAny> value, WriteBarrierMode mode) {
  underlying_iterator_next_.store(this, value, mode);
}

Tagged<JSReceiver> JSIteratorMapHelper::mapper() const {
  return mapper_.load();
}

void JSIteratorMapHelper::set_mapper(Tagged<JSReceiver> value,
                                     WriteBarrierMode mode) {
  mapper_.store(this, value, mode);
}

Tagged<Number> JSIteratorMapHelper::counter() const { return counter_.load(); }

void JSIteratorMapHelper::set_counter(Tagged<Number> value,
                                      WriteBarrierMode mode) {
  counter_.store(this, value, mode);
}

Tagged<JSReceiver> JSIteratorFilterHelper::predicate() const {
  return predicate_.load();
}

void JSIteratorFilterHelper::set_predicate(Tagged<JSReceiver> value,
                                           WriteBarrierMode mode) {
  predicate_.store(this, value, mode);
}

Tagged<Number> JSIteratorFilterHelper::counter() const {
  return counter_.load();
}

void JSIteratorFilterHelper::set_counter(Tagged<Number> value,
                                         WriteBarrierMode mode) {
  counter_.store(this, value, mode);
}

Tagged<Number> JSIteratorTakeHelper::remaining() const {
  return remaining_.load();
}

void JSIteratorTakeHelper::set_remaining(Tagged<Number> value,
                                         WriteBarrierMode mode) {
  remaining_.store(this, value, mode);
}

Tagged<Number> JSIteratorDropHelper::remaining() const {
  return remaining_.load();
}

void JSIteratorDropHelper::set_remaining(Tagged<Number> value,
                                         WriteBarrierMode mode) {
  remaining_.store(this, value, mode);
}

Tagged<JSReceiver> JSIteratorFlatMapHelper::mapper() const {
  return mapper_.load();
}

void JSIteratorFlatMapHelper::set_mapper(Tagged<JSReceiver> value,
                                         WriteBarrierMode mode) {
  mapper_.store(this, value, mode);
}

Tagged<Number> JSIteratorFlatMapHelper::counter() const {
  return counter_.load();
}

void JSIteratorFlatMapHelper::set_counter(Tagged<Number> value,
                                          WriteBarrierMode mode) {
  counter_.store(this, value, mode);
}

Tagged<JSReceiver> JSIteratorFlatMapHelper::inner_iterator_object() const {
  return inner_iterator_object_.load();
}

void JSIteratorFlatMapHelper::set_inner_iterator_object(
    Tagged<JSReceiver> value, WriteBarrierMode mode) {
  inner_iterator_object_.store(this, value, mode);
}

Tagged<JSAny> JSIteratorFlatMapHelper::inner_iterator_next() const {
  return inner_iterator_next_.load();
}

void JSIteratorFlatMapHelper::set_inner_iterator_next(Tagged<JSAny> value,
                                                      WriteBarrierMode mode) {
  inner_iterator_next_.store(this, value, mode);
}

Tagged<FixedArray> JSIteratorConcatHelper::iterables() const {
  return iterables_.load();
}

void JSIteratorConcatHelper::set_iterables(Tagged<FixedArray> value,
                                           WriteBarrierMode mode) {
  iterables_.store(this, value, mode);
}

Tagged<Smi> JSIteratorConcatHelper::current() const { return current_.load(); }

void JSIteratorConcatHelper::set_current(Tagged<Smi> value) {
  current_.store(this, value);
}

Tagged<FixedArray> JSIteratorZipHelper::underlying_iterators() const {
  return underlying_iterators_.load();
}

void JSIteratorZipHelper::set_underlying_iterators(Tagged<FixedArray> value,
                                                   WriteBarrierMode mode) {
  underlying_iterators_.store(this, value, mode);
}

JSIteratorZipHelperMode JSIteratorZipHelper::mode() const {
  return static_cast<JSIteratorZipHelperMode>(mode_.load().value());
}

void JSIteratorZipHelper::set_mode(JSIteratorZipHelperMode value) {
  mode_.store(this, Smi::From31BitPattern(static_cast<int>(value)));
}

Tagged<Smi> JSIteratorZipHelper::active_count() const {
  return active_count_.load();
}

void JSIteratorZipHelper::set_active_count(Tagged<Smi> value) {
  active_count_.store(this, value);
}

Tagged<FixedArray> JSIteratorZipHelper::padding() const {
  return padding_.load();
}

void JSIteratorZipHelper::set_padding(Tagged<FixedArray> value,
                                      WriteBarrierMode mode) {
  padding_.store(this, value, mode);
}

Tagged<FixedArray> JSIteratorZipKeyedHelper::keys() const {
  return keys_.load();
}

void JSIteratorZipKeyedHelper::set_keys(Tagged<FixedArray> value,
                                        WriteBarrierMode mode) {
  keys_.store(this, value, mode);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_ITERATOR_HELPERS_INL_H_
