// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MODULE_H_
#define V8_OBJECTS_MODULE_H_

#include "src/objects/fixed-array.h"
#include "src/objects/js-objects.h"
#include "src/objects/objects.h"
#include "src/objects/struct.h"
#include "torque-generated/field-offsets-tq.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

template <typename T>
class Handle;
class Isolate;
class JSModuleNamespace;
class SourceTextModuleDescriptor;
class SourceTextModuleInfo;
class SourceTextModuleInfoEntry;
class String;
class Zone;

// Module is the base class for ECMAScript module types, roughly corresponding
// to Abstract Module Record.
// https://tc39.github.io/ecma262/#sec-abstract-module-records
class Module : public HeapObject {
 public:
  NEVER_READ_ONLY_SPACE
  DECL_CAST(Module)
  DECL_VERIFIER(Module)
  DECL_PRINTER(Module)

  // The complete export table, mapping an export name to its cell.
  DECL_ACCESSORS(exports, ObjectHashTable)

  // Hash for this object (a random non-zero Smi).
  DECL_INT_ACCESSORS(hash)

  // Status.
  DECL_INT_ACCESSORS(status)
  enum Status {
    // Order matters!
    kUninstantiated,
    kPreInstantiating,
    kInstantiating,
    kInstantiated,
    kEvaluating,
    kEvaluated,
    kErrored
  };

  // The namespace object (or undefined).
  DECL_ACCESSORS(module_namespace, HeapObject)

  // The exception in the case {status} is kErrored.
  Object GetException();
  DECL_ACCESSORS(exception, Object)

  // Implementation of spec operation ModuleDeclarationInstantiation.
  // Returns false if an exception occurred during instantiation, true
  // otherwise. (In the case where the callback throws an exception, that
  // exception is propagated.)
  static V8_WARN_UNUSED_RESULT bool Instantiate(
      Isolate* isolate, Handle<Module> module, v8::Local<v8::Context> context,
      v8::Module::ResolveCallback callback);

  // Implementation of spec operation ModuleEvaluation.
  static V8_WARN_UNUSED_RESULT MaybeHandle<Object> Evaluate(
      Isolate* isolate, Handle<Module> module);

  // Get the namespace object for [module].  If it doesn't exist yet, it is
  // created.
  static Handle<JSModuleNamespace> GetModuleNamespace(Isolate* isolate,
                                                      Handle<Module> module);

// Layout description.
  DEFINE_FIELD_OFFSET_CONSTANTS(Struct::kHeaderSize,
                                TORQUE_GENERATED_MODULE_FIELDS)

  using BodyDescriptor =
      FixedBodyDescriptor<kExportsOffset, kHeaderSize, kHeaderSize>;

 protected:
  friend class Factory;

  // The [must_resolve] argument indicates whether or not an exception should be
  // thrown in case the module does not provide an export named [name]
  // (including when a cycle is detected).  An exception is always thrown in the
  // case of conflicting star exports.
  //
  // If [must_resolve] is true, a null result indicates an exception. If
  // [must_resolve] is false, a null result may or may not indicate an
  // exception (so check manually!).
  class ResolveSet;
  static V8_WARN_UNUSED_RESULT MaybeHandle<Cell> ResolveExport(
      Isolate* isolate, Handle<Module> module, Handle<String> module_specifier,
      Handle<String> export_name, MessageLocation loc, bool must_resolve,
      ResolveSet* resolve_set);

  static V8_WARN_UNUSED_RESULT bool PrepareInstantiate(
      Isolate* isolate, Handle<Module> module, v8::Local<v8::Context> context,
      v8::Module::ResolveCallback callback);
  static V8_WARN_UNUSED_RESULT bool FinishInstantiate(
      Isolate* isolate, Handle<Module> module,
      ZoneForwardList<Handle<SourceTextModule>>* stack, unsigned* dfs_index,
      Zone* zone);

  static V8_WARN_UNUSED_RESULT MaybeHandle<Object> Evaluate(
      Isolate* isolate, Handle<Module> module,
      ZoneForwardList<Handle<SourceTextModule>>* stack, unsigned* dfs_index);

  // Set module's status back to kUninstantiated and reset other internal state.
  // This is used when instantiation fails.
  static void Reset(Isolate* isolate, Handle<Module> module);
  static void ResetGraph(Isolate* isolate, Handle<Module> module);

  // To set status to kErrored, RecordError should be used.
  void SetStatus(Status status);
  void RecordError(Isolate* isolate);

#ifdef DEBUG
  // For --trace-module-status.
  void PrintStatusTransition(Status new_status);
#endif  // DEBUG

  OBJECT_CONSTRUCTORS(Module, HeapObject);
};

// When importing a module namespace (import * as foo from "bar"), a
// JSModuleNamespace object (representing module "bar") is created and bound to
// the declared variable (foo).  A module can have at most one namespace object.
class JSModuleNamespace : public JSObject {
 public:
  DECL_CAST(JSModuleNamespace)
  DECL_PRINTER(JSModuleNamespace)
  DECL_VERIFIER(JSModuleNamespace)

  // The actual module whose namespace is being represented.
  DECL_ACCESSORS(module, Module)

  // Retrieve the value exported by [module] under the given [name]. If there is
  // no such export, return Just(undefined). If the export is uninitialized,
  // schedule an exception and return Nothing.
  V8_WARN_UNUSED_RESULT MaybeHandle<Object> GetExport(Isolate* isolate,
                                                      Handle<String> name);

  // Return the (constant) property attributes for the referenced property,
  // which is assumed to correspond to an export. If the export is
  // uninitialized, schedule an exception and return Nothing.
  static V8_WARN_UNUSED_RESULT Maybe<PropertyAttributes> GetPropertyAttributes(
      LookupIterator* it);

  // In-object fields.
  enum {
    kToStringTagFieldIndex,
    kInObjectFieldCount,
  };

  // Layout description.
  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                TORQUE_GENERATED_JSMODULE_NAMESPACE_FIELDS)

  // We need to include in-object fields
  // TODO(v8:8944): improve handling of in-object fields
  static constexpr int kSize =
      kHeaderSize + (kTaggedSize * kInObjectFieldCount);

  OBJECT_CONSTRUCTORS(JSModuleNamespace, JSObject);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_MODULE_H_
