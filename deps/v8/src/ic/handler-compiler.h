// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_HANDLER_COMPILER_H_
#define V8_IC_HANDLER_COMPILER_H_

#include "src/ic/access-compiler.h"
#include "src/ic/ic-state.h"

namespace v8 {
namespace internal {

class CallOptimization;

class PropertyHandlerCompiler : public PropertyAccessCompiler {
 public:
  static Handle<Code> Find(Handle<Name> name, Handle<Map> map, Code::Kind kind);

 protected:
  PropertyHandlerCompiler(Isolate* isolate, Code::Kind kind, Handle<Map> map,
                          Handle<JSObject> holder)
      : PropertyAccessCompiler(isolate, kind), map_(map), holder_(holder) {}

  virtual ~PropertyHandlerCompiler() {}

  virtual Register FrontendHeader(Register object_reg, Handle<Name> name,
                                  Label* miss) {
    UNREACHABLE();
    return receiver();
  }

  virtual void FrontendFooter(Handle<Name> name, Label* miss) { UNREACHABLE(); }

  // Frontend loads from receiver(), returns holder register which may be
  // different.
  Register Frontend(Handle<Name> name);

  // When FLAG_vector_ics is true, handlers that have the possibility of missing
  // will need to save and pass these to miss handlers.
  void PushVectorAndSlot() { PushVectorAndSlot(vector(), slot()); }
  void PushVectorAndSlot(Register vector, Register slot);
  void PopVectorAndSlot() { PopVectorAndSlot(vector(), slot()); }
  void PopVectorAndSlot(Register vector, Register slot);

  void DiscardVectorAndSlot();

  // TODO(verwaest): Make non-static.
  static void GenerateApiAccessorCall(MacroAssembler* masm,
                                      const CallOptimization& optimization,
                                      Handle<Map> receiver_map,
                                      Register receiver, Register scratch,
                                      bool is_store, Register store_parameter,
                                      Register accessor_holder,
                                      int accessor_index);

  // Helper function used to check that the dictionary doesn't contain
  // the property. This function may return false negatives, so miss_label
  // must always call a backup property check that is complete.
  // This function is safe to call if the receiver has fast properties.
  // Name must be unique and receiver must be a heap object.
  static void GenerateDictionaryNegativeLookup(MacroAssembler* masm,
                                               Label* miss_label,
                                               Register receiver,
                                               Handle<Name> name, Register r0,
                                               Register r1);

  // Generate code to check that a global property cell is empty. Create
  // the property cell at compilation time if no cell exists for the
  // property.
  static void GenerateCheckPropertyCell(MacroAssembler* masm,
                                        Handle<JSGlobalObject> global,
                                        Handle<Name> name, Register scratch,
                                        Label* miss);

  // Generates check that current native context has the same access rights
  // as the given |native_context_cell|.
  // If |compare_native_contexts_only| is true then access check is considered
  // passed if the execution-time native context is equal to contents of
  // |native_context_cell|.
  // If |compare_native_contexts_only| is false then access check is considered
  // passed if the execution-time native context is equal to contents of
  // |native_context_cell| or security tokens of both contexts are equal.
  void GenerateAccessCheck(Handle<WeakCell> native_context_cell,
                           Register scratch1, Register scratch2, Label* miss,
                           bool compare_native_contexts_only);

  // Generates code that verifies that the property holder has not changed
  // (checking maps of objects in the prototype chain for fast and global
  // objects or doing negative lookup for slow objects, ensures that the
  // property cells for global objects are still empty) and checks that the map
  // of the holder has not changed. If necessary the function also generates
  // code for security check in case of global object holders. Helps to make
  // sure that the current IC is still valid.
  //
  // The scratch and holder registers are always clobbered, but the object
  // register is only clobbered if it the same as the holder register. The
  // function returns a register containing the holder - either object_reg or
  // holder_reg.
  Register CheckPrototypes(Register object_reg, Register holder_reg,
                           Register scratch1, Register scratch2,
                           Handle<Name> name, Label* miss);

  Handle<Code> GetCode(Code::Kind kind, Handle<Name> name);
  Handle<Map> map() const { return map_; }
  Handle<JSObject> holder() const { return holder_; }

 private:
  Handle<Map> map_;
  Handle<JSObject> holder_;
};


class NamedLoadHandlerCompiler : public PropertyHandlerCompiler {
 public:
  NamedLoadHandlerCompiler(Isolate* isolate, Handle<Map> map,
                           Handle<JSObject> holder)
      : PropertyHandlerCompiler(isolate, Code::LOAD_IC, map, holder) {}

  virtual ~NamedLoadHandlerCompiler() {}

  Handle<Code> CompileLoadCallback(Handle<Name> name,
                                   const CallOptimization& call_optimization,
                                   int accessor_index, Handle<Code> slow_stub);

  static void GenerateLoadViaGetterForDeopt(MacroAssembler* masm);

 protected:
  virtual Register FrontendHeader(Register object_reg, Handle<Name> name,
                                  Label* miss);

  virtual void FrontendFooter(Handle<Name> name, Label* miss);

 private:
  Register scratch3() { return registers_[4]; }
};


class NamedStoreHandlerCompiler : public PropertyHandlerCompiler {
 public:
  // All store handlers use StoreWithVectorDescriptor calling convention.
  typedef StoreWithVectorDescriptor Descriptor;

  explicit NamedStoreHandlerCompiler(Isolate* isolate, Handle<Map> map,
                                     Handle<JSObject> holder)
      : PropertyHandlerCompiler(isolate, Code::STORE_IC, map, holder) {
#ifdef DEBUG
    if (Descriptor::kPassLastArgsOnStack) {
      ZapStackArgumentsRegisterAliases();
    }
#endif
  }

  virtual ~NamedStoreHandlerCompiler() {}

  void ZapStackArgumentsRegisterAliases();

  Handle<Code> CompileStoreCallback(Handle<JSObject> object, Handle<Name> name,
                                    Handle<AccessorInfo> callback,
                                    LanguageMode language_mode);
  Handle<Code> CompileStoreCallback(Handle<JSObject> object, Handle<Name> name,
                                    const CallOptimization& call_optimization,
                                    int accessor_index, Handle<Code> slow_stub);
  Handle<Code> CompileStoreViaSetter(Handle<JSObject> object, Handle<Name> name,
                                     int accessor_index,
                                     int expected_arguments);

  static void GenerateStoreViaSetter(MacroAssembler* masm, Handle<Map> map,
                                     Register receiver, Register holder,
                                     int accessor_index, int expected_arguments,
                                     Register scratch);

  static void GenerateStoreViaSetterForDeopt(MacroAssembler* masm) {
    GenerateStoreViaSetter(masm, Handle<Map>::null(), no_reg, no_reg, -1, -1,
                           no_reg);
  }

 protected:
  virtual Register FrontendHeader(Register object_reg, Handle<Name> name,
                                  Label* miss);

  virtual void FrontendFooter(Handle<Name> name, Label* miss);
  void GenerateRestoreName(Label* label, Handle<Name> name);

 private:
  static Register value();
};

}  // namespace internal
}  // namespace v8

#endif  // V8_IC_HANDLER_COMPILER_H_
