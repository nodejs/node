// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MODULE_H_
#define V8_OBJECTS_MODULE_H_

#include "src/objects.h"
#include "src/objects/fixed-array.h"
#include "src/objects/js-objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

template <typename T>
class Handle;
class Isolate;
class JSModuleNamespace;
class ModuleDescriptor;
class ModuleInfo;
class ModuleInfoEntry;
class String;
class Zone;

// The runtime representation of an ECMAScript module.
class Module : public Struct, public NeverReadOnlySpaceObject {
 public:
  DECL_CAST(Module)
  DECL_VERIFIER(Module)
  DECL_PRINTER(Module)

  // The code representing this module, or an abstraction thereof.
  // This is either a SharedFunctionInfo, a JSFunction, a JSGeneratorObject, or
  // a ModuleInfo, depending on the state (status) the module is in. See
  // Module::ModuleVerify() for the precise invariant.
  DECL_ACCESSORS(code, Object)

  // Arrays of cells corresponding to regular exports and regular imports.
  // A cell's position in the array is determined by the cell index of the
  // associated module entry (which coincides with the variable index of the
  // associated variable).
  DECL_ACCESSORS(regular_exports, FixedArray)
  DECL_ACCESSORS(regular_imports, FixedArray)

  // The complete export table, mapping an export name to its cell.
  // TODO(neis): We may want to remove the regular exports from the table.
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

  // The exception in the case {status} is kErrored.
  Object* GetException();

  // The shared function info in case {status} is not kEvaluating, kEvaluated or
  // kErrored.
  SharedFunctionInfo* GetSharedFunctionInfo() const;

  // The namespace object (or undefined).
  DECL_ACCESSORS(module_namespace, HeapObject)

  // Modules imported or re-exported by this module.
  // Corresponds 1-to-1 to the module specifier strings in
  // ModuleInfo::module_requests.
  DECL_ACCESSORS(requested_modules, FixedArray)

  // [script]: Script from which the module originates.
  DECL_ACCESSORS(script, Script)

  // The value of import.meta inside of this module.
  // Lazily initialized on first access. It's the hole before first access and
  // a JSObject afterwards.
  DECL_ACCESSORS(import_meta, Object)

  // Get the ModuleInfo associated with the code.
  inline ModuleInfo* info() const;

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

  Cell* GetCell(int cell_index);
  static Handle<Object> LoadVariable(Isolate* isolate, Handle<Module> module,
                                     int cell_index);
  static void StoreVariable(Handle<Module> module, int cell_index,
                            Handle<Object> value);

  static int ImportIndex(int cell_index);
  static int ExportIndex(int cell_index);

  // Get the namespace object for [module_request] of [module].  If it doesn't
  // exist yet, it is created.
  static Handle<JSModuleNamespace> GetModuleNamespace(Isolate* isolate,
                                                      Handle<Module> module,
                                                      int module_request);

  // Get the namespace object for [module].  If it doesn't exist yet, it is
  // created.
  static Handle<JSModuleNamespace> GetModuleNamespace(Isolate* isolate,
                                                      Handle<Module> module);

  static const int kCodeOffset = HeapObject::kHeaderSize;
  static const int kExportsOffset = kCodeOffset + kPointerSize;
  static const int kRegularExportsOffset = kExportsOffset + kPointerSize;
  static const int kRegularImportsOffset = kRegularExportsOffset + kPointerSize;
  static const int kHashOffset = kRegularImportsOffset + kPointerSize;
  static const int kModuleNamespaceOffset = kHashOffset + kPointerSize;
  static const int kRequestedModulesOffset =
      kModuleNamespaceOffset + kPointerSize;
  static const int kStatusOffset = kRequestedModulesOffset + kPointerSize;
  static const int kDfsIndexOffset = kStatusOffset + kPointerSize;
  static const int kDfsAncestorIndexOffset = kDfsIndexOffset + kPointerSize;
  static const int kExceptionOffset = kDfsAncestorIndexOffset + kPointerSize;
  static const int kScriptOffset = kExceptionOffset + kPointerSize;
  static const int kImportMetaOffset = kScriptOffset + kPointerSize;
  static const int kSize = kImportMetaOffset + kPointerSize;

 private:
  friend class Factory;

  DECL_ACCESSORS(exception, Object)

  // TODO(neis): Don't store those in the module object?
  DECL_INT_ACCESSORS(dfs_index)
  DECL_INT_ACCESSORS(dfs_ancestor_index)

  // Helpers for Instantiate and Evaluate.

  static void CreateExport(Isolate* isolate, Handle<Module> module,
                           int cell_index, Handle<FixedArray> names);
  static void CreateIndirectExport(Isolate* isolate, Handle<Module> module,
                                   Handle<String> name,
                                   Handle<ModuleInfoEntry> entry);

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
  static V8_WARN_UNUSED_RESULT MaybeHandle<Cell> ResolveImport(
      Isolate* isolate, Handle<Module> module, Handle<String> name,
      int module_request, MessageLocation loc, bool must_resolve,
      ResolveSet* resolve_set);

  static V8_WARN_UNUSED_RESULT MaybeHandle<Cell> ResolveExportUsingStarExports(
      Isolate* isolate, Handle<Module> module, Handle<String> module_specifier,
      Handle<String> export_name, MessageLocation loc, bool must_resolve,
      ResolveSet* resolve_set);

  static V8_WARN_UNUSED_RESULT bool PrepareInstantiate(
      Isolate* isolate, Handle<Module> module, v8::Local<v8::Context> context,
      v8::Module::ResolveCallback callback);
  static V8_WARN_UNUSED_RESULT bool FinishInstantiate(
      Isolate* isolate, Handle<Module> module,
      ZoneForwardList<Handle<Module>>* stack, unsigned* dfs_index, Zone* zone);
  static V8_WARN_UNUSED_RESULT bool RunInitializationCode(
      Isolate* isolate, Handle<Module> module);

  static V8_WARN_UNUSED_RESULT MaybeHandle<Object> Evaluate(
      Isolate* isolate, Handle<Module> module,
      ZoneForwardList<Handle<Module>>* stack, unsigned* dfs_index);

  static V8_WARN_UNUSED_RESULT bool MaybeTransitionComponent(
      Isolate* isolate, Handle<Module> module,
      ZoneForwardList<Handle<Module>>* stack, Status new_status);

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

  DISALLOW_IMPLICIT_CONSTRUCTORS(Module);
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

  static const int kModuleOffset = JSObject::kHeaderSize;
  static const int kHeaderSize = kModuleOffset + kPointerSize;

  static const int kSize = kHeaderSize + kPointerSize * kInObjectFieldCount;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSModuleNamespace);
};

// ModuleInfo is to ModuleDescriptor what ScopeInfo is to Scope.
class ModuleInfo : public FixedArray {
 public:
  DECL_CAST(ModuleInfo)

  static Handle<ModuleInfo> New(Isolate* isolate, Zone* zone,
                                ModuleDescriptor* descr);

  inline FixedArray* module_requests() const;
  inline FixedArray* special_exports() const;
  inline FixedArray* regular_exports() const;
  inline FixedArray* regular_imports() const;
  inline FixedArray* namespace_imports() const;
  inline FixedArray* module_request_positions() const;

  // Accessors for [regular_exports].
  int RegularExportCount() const;
  String* RegularExportLocalName(int i) const;
  int RegularExportCellIndex(int i) const;
  FixedArray* RegularExportExportNames(int i) const;

#ifdef DEBUG
  inline bool Equals(ModuleInfo* other) const;
#endif

 private:
  friend class Factory;
  friend class ModuleDescriptor;
  enum {
    kModuleRequestsIndex,
    kSpecialExportsIndex,
    kRegularExportsIndex,
    kNamespaceImportsIndex,
    kRegularImportsIndex,
    kModuleRequestPositionsIndex,
    kLength
  };
  enum {
    kRegularExportLocalNameOffset,
    kRegularExportCellIndexOffset,
    kRegularExportExportNamesOffset,
    kRegularExportLength
  };
  DISALLOW_IMPLICIT_CONSTRUCTORS(ModuleInfo);
};

class ModuleInfoEntry : public Struct {
 public:
  DECL_CAST(ModuleInfoEntry)
  DECL_PRINTER(ModuleInfoEntry)
  DECL_VERIFIER(ModuleInfoEntry)

  DECL_ACCESSORS(export_name, Object)
  DECL_ACCESSORS(local_name, Object)
  DECL_ACCESSORS(import_name, Object)
  DECL_INT_ACCESSORS(module_request)
  DECL_INT_ACCESSORS(cell_index)
  DECL_INT_ACCESSORS(beg_pos)
  DECL_INT_ACCESSORS(end_pos)

  static Handle<ModuleInfoEntry> New(Isolate* isolate,
                                     Handle<Object> export_name,
                                     Handle<Object> local_name,
                                     Handle<Object> import_name,
                                     int module_request, int cell_index,
                                     int beg_pos, int end_pos);

  static const int kExportNameOffset = HeapObject::kHeaderSize;
  static const int kLocalNameOffset = kExportNameOffset + kPointerSize;
  static const int kImportNameOffset = kLocalNameOffset + kPointerSize;
  static const int kModuleRequestOffset = kImportNameOffset + kPointerSize;
  static const int kCellIndexOffset = kModuleRequestOffset + kPointerSize;
  static const int kBegPosOffset = kCellIndexOffset + kPointerSize;
  static const int kEndPosOffset = kBegPosOffset + kPointerSize;
  static const int kSize = kEndPosOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ModuleInfoEntry);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_MODULE_H_
