// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SYNTHETIC_MODULE_H_
#define V8_OBJECTS_SYNTHETIC_MODULE_H_

#include "src/objects/module.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// The runtime representation of a Synthetic Module Record, a module that can be
// instantiated by an embedder with embedder-defined exports and evaluation
// steps.
// https://heycam.github.io/webidl/#synthetic-module-records
class SyntheticModule
    : public TorqueGeneratedSyntheticModule<SyntheticModule, Module> {
 public:
  NEVER_READ_ONLY_SPACE
  DECL_VERIFIER(SyntheticModule)
  DECL_PRINTER(SyntheticModule)

  static void SetExport(Isolate* isolate, Handle<SyntheticModule> module,
                        Handle<String> export_name,
                        Handle<Object> export_value);

  using BodyDescriptor = SubclassBodyDescriptor<
      Module::BodyDescriptor,
      FixedBodyDescriptor<kExportNamesOffset, kSize, kSize>>;

 private:
  friend class Module;

  static V8_WARN_UNUSED_RESULT MaybeHandle<Cell> ResolveExport(
      Isolate* isolate, Handle<SyntheticModule> module,
      Handle<String> module_specifier, Handle<String> export_name,
      MessageLocation loc, bool must_resolve);

  static V8_WARN_UNUSED_RESULT bool PrepareInstantiate(
      Isolate* isolate, Handle<SyntheticModule> module,
      v8::Local<v8::Context> context, v8::Module::ResolveCallback callback);
  static V8_WARN_UNUSED_RESULT bool FinishInstantiate(
      Isolate* isolate, Handle<SyntheticModule> module);

  static V8_WARN_UNUSED_RESULT MaybeHandle<Object> Evaluate(
      Isolate* isolate, Handle<SyntheticModule> module);

  TQ_OBJECT_CONSTRUCTORS(SyntheticModule)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SYNTHETIC_MODULE_H_
