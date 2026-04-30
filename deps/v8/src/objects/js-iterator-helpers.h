// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_ITERATOR_HELPERS_H_
#define V8_OBJECTS_JS_ITERATOR_HELPERS_H_

#include "src/objects/js-objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class Boolean;

enum class JSIteratorHelperState {
  kSuspendedStart,
  kSuspendedYield,
  kExecuting,
  kCompleted
};

enum class JSIteratorZipHelperMode { kShortest, kLongest, kStrict };

V8_EXPORT_PRIVATE const char* JSIteratorHelperStateToString(
    JSIteratorHelperState state);

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                           JSIteratorHelperState state);

V8_EXPORT_PRIVATE const char* JSIteratorZipHelperModeToString(
    JSIteratorZipHelperMode mode);

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os,
                                           JSIteratorZipHelperMode mode);

// Iterator helpers are iterators that transform an underlying iterator in some
// way. They are specified as spec generators. That is, the spec defines the
// body of iterator helpers using algorithm steps with yields (like JS
// generators) packaged in an Abstract Closure, and then makes a generator
// object internally. Generator machinery such as GeneratorResume [1] are then
// used to specify %IteratorHelperPrototype%.{next,return}. While this aids
// understandability of the specification, it is not conducive to ease of
// implementation or performance in V8.
//
// Instead, each iterator helper is implemented as an iterator directly, with
// JSIteratorHelper acting as a superclass to multiplex the various kinds of
// helpers.
//
// Each helper has its own Torque class to hold the state it needs. (In the
// spec, the state is captured in the Abstract Closures.) The classes are named
// after the name of the method that produces them. E.g., the iterator helper
// returned by Iterator.prototype.map is named JSIteratorMapHelper, and has
// fields for the underlying iterator, the mapper function, and a counter.
//
// The algorithm steps in the body Abstract Closure in the specification is
// implemented directly as next() (and return(), if necessary) builtin
// methods. E.g., the map helper's body is implemented as
// Builtin::kIteratorMapHelperNext.
//
// All iterator helper objects have %IteratorHelperPrototype% as their
// [[Prototype]]. The implementations of %IteratorHelperPrototype%.{next,return}
// multiplex, typeswitching over all known iterator helpers and manually calling
// their next() (and return(), if necessary) builtins. E.g., Calling next() on
// JSIteratorMapHelper would ultimately call Builtin::kIteratorMapHelperNext.
//
// [1] https://tc39.es/ecma262/#sec-generatorresume

// The superclass of all iterator helpers.
V8_OBJECT class JSIteratorHelper : public JSObject {
 public:
  inline JSIteratorHelperState state() const;
  inline void set_state(JSIteratorHelperState value);

  void JSIteratorHelperPrintHeader(std::ostream& os, const char* helper_name);

 public:
  // SmiTagged<JSIteratorHelperState>.
  TaggedMember<Smi> state_;
} V8_OBJECT_END;

// The superclass of iterator helpers that have a single underlying iterator.
V8_OBJECT class JSIteratorHelperSimple : public JSIteratorHelper {
 public:
  inline Tagged<JSReceiver> underlying_iterator_object() const;
  inline void set_underlying_iterator_object(
      Tagged<JSReceiver> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<JSAny> underlying_iterator_next() const;
  inline void set_underlying_iterator_next(
      Tagged<JSAny> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  void JSIteratorHelperSimplePrintHeader(std::ostream& os,
                                         const char* helper_name);

 public:
  TaggedMember<JSReceiver> underlying_iterator_object_;
  TaggedMember<JSAny> underlying_iterator_next_;
} V8_OBJECT_END;

// The iterator helper returned by Iterator.prototype.map.
V8_OBJECT class JSIteratorMapHelper final : public JSIteratorHelperSimple {
 public:
  inline Tagged<JSReceiver> mapper() const;
  inline void set_mapper(Tagged<JSReceiver> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Number> counter() const;
  inline void set_counter(Tagged<Number> value,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_PRINTER(JSIteratorMapHelper)
  DECL_VERIFIER(JSIteratorMapHelper)

 public:
  TaggedMember<JSReceiver> mapper_;
  TaggedMember<Number> counter_;
} V8_OBJECT_END;

// The iterator helper returned by Iterator.prototype.filter.
V8_OBJECT class JSIteratorFilterHelper final : public JSIteratorHelperSimple {
 public:
  inline Tagged<JSReceiver> predicate() const;
  inline void set_predicate(Tagged<JSReceiver> value,
                            WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Number> counter() const;
  inline void set_counter(Tagged<Number> value,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_PRINTER(JSIteratorFilterHelper)
  DECL_VERIFIER(JSIteratorFilterHelper)

 public:
  TaggedMember<JSReceiver> predicate_;
  TaggedMember<Number> counter_;
} V8_OBJECT_END;

// The iterator helper returned by Iterator.prototype.take.
V8_OBJECT class JSIteratorTakeHelper final : public JSIteratorHelperSimple {
 public:
  inline Tagged<Number> remaining() const;
  inline void set_remaining(Tagged<Number> value,
                            WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_PRINTER(JSIteratorTakeHelper)
  DECL_VERIFIER(JSIteratorTakeHelper)

 public:
  TaggedMember<Number> remaining_;
} V8_OBJECT_END;

// The iterator helper returned by Iterator.prototype.drop.
V8_OBJECT class JSIteratorDropHelper final : public JSIteratorHelperSimple {
 public:
  inline Tagged<Number> remaining() const;
  inline void set_remaining(Tagged<Number> value,
                            WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_PRINTER(JSIteratorDropHelper)
  DECL_VERIFIER(JSIteratorDropHelper)

 public:
  TaggedMember<Number> remaining_;
} V8_OBJECT_END;

// The iterator helper returned by Iterator.prototype.flatMap.
V8_OBJECT class JSIteratorFlatMapHelper final : public JSIteratorHelperSimple {
 public:
  inline Tagged<JSReceiver> mapper() const;
  inline void set_mapper(Tagged<JSReceiver> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Number> counter() const;
  inline void set_counter(Tagged<Number> value,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<JSReceiver> inner_iterator_object() const;
  inline void set_inner_iterator_object(
      Tagged<JSReceiver> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<JSAny> inner_iterator_next() const;
  inline void set_inner_iterator_next(
      Tagged<JSAny> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_PRINTER(JSIteratorFlatMapHelper)
  DECL_VERIFIER(JSIteratorFlatMapHelper)

 public:
  TaggedMember<JSReceiver> mapper_;
  TaggedMember<Number> counter_;
  TaggedMember<JSReceiver> inner_iterator_object_;
  TaggedMember<JSAny> inner_iterator_next_;
} V8_OBJECT_END;

// The iterator helper returned by Iterator.concat.
V8_OBJECT class JSIteratorConcatHelper final : public JSIteratorHelperSimple {
 public:
  inline Tagged<FixedArray> iterables() const;
  inline void set_iterables(Tagged<FixedArray> value,
                            WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Smi> current() const;
  inline void set_current(Tagged<Smi> value);

  DECL_PRINTER(JSIteratorConcatHelper)
  DECL_VERIFIER(JSIteratorConcatHelper)

 public:
  TaggedMember<FixedArray> iterables_;
  TaggedMember<Smi> current_;
} V8_OBJECT_END;

// The iterator helper returned by Iterator.zip and Iterator.zipKeyed.
V8_OBJECT class JSIteratorZipHelper final : public JSIteratorHelper {
 public:
  inline Tagged<FixedArray> underlying_iterators() const;
  inline void set_underlying_iterators(
      Tagged<FixedArray> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline JSIteratorZipHelperMode mode() const;
  inline void set_mode(JSIteratorZipHelperMode value);

  inline Tagged<Smi> active_count() const;
  inline void set_active_count(Tagged<Smi> value);

  inline Tagged<FixedArray> padding() const;
  inline void set_padding(Tagged<FixedArray> value,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_PRINTER(JSIteratorZipHelper)
  DECL_VERIFIER(JSIteratorZipHelper)

 public:
  TaggedMember<FixedArray> underlying_iterators_;
  // SmiTagged<JSIteratorZipHelperMode>.
  TaggedMember<Smi> mode_;
  TaggedMember<Smi> active_count_;
  TaggedMember<FixedArray> padding_;
} V8_OBJECT_END;

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_ITERATOR_HELPERS_H_
