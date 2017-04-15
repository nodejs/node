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

enum ReturnHolder { RETURN_HOLDER, DONT_RETURN_ANYTHING };

class PropertyHandlerCompiler : public PropertyAccessCompiler {
 public:
  static Handle<Code> Find(Handle<Name> name, Handle<Map> map, Code::Kind kind,
                           CacheHolderFlag cache_holder);

 protected:
  PropertyHandlerCompiler(Isolate* isolate, Code::Kind kind, Handle<Map> map,
                          Handle<JSObject> holder, CacheHolderFlag cache_holder)
      : PropertyAccessCompiler(isolate, kind, cache_holder),
        map_(map),
        holder_(holder) {}

  virtual ~PropertyHandlerCompiler() {}

  virtual Register FrontendHeader(Register object_reg, Handle<Name> name,
                                  Label* miss, ReturnHolder return_what) {
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
                           Handle<Name> name, Label* miss,
                           ReturnHolder return_what);

  Handle<Code> GetCode(Code::Kind kind, Handle<Name> name);
  void set_holder(Handle<JSObject> holder) { holder_ = holder; }
  Handle<Map> map() const { return map_; }
  void set_map(Handle<Map> map) { map_ = map; }
  Handle<JSObject> holder() const { return holder_; }

 private:
  Handle<Map> map_;
  Handle<JSObject> holder_;
};


class NamedLoadHandlerCompiler : public PropertyHandlerCompiler {
 public:
  NamedLoadHandlerCompiler(Isolate* isolate, Handle<Map> map,
                           Handle<JSObject> holder,
                           CacheHolderFlag cache_holder)
      : PropertyHandlerCompiler(isolate, Code::LOAD_IC, map, holder,
                                cache_holder) {}

  virtual ~NamedLoadHandlerCompiler() {}

  Handle<Code> CompileLoadCallback(Handle<Name> name,
                                   Handle<AccessorInfo> callback,
                                   Handle<Code> slow_stub);

  Handle<Code> CompileLoadCallback(Handle<Name> name,
                                   const CallOptimization& call_optimization,
                                   int accessor_index, Handle<Code> slow_stub);

  // The LookupIterator is used to perform a lookup behind the interceptor. If
  // the iterator points to a LookupIterator::PROPERTY, its access will be
  // inlined.
  Handle<Code> CompileLoadInterceptor(LookupIterator* it);

  Handle<Code> CompileLoadViaGetter(Handle<Name> name, int accessor_index,
                                    int expected_arguments);

  Handle<Code> CompileLoadGlobal(Handle<PropertyCell> cell, Handle<Name> name,
                                 bool is_configurable);

  static void GenerateLoadViaGetter(MacroAssembler* masm, Handle<Map> map,
                                    Register receiver, Register holder,
                                    int accessor_index, int expected_arguments,
                                    Register scratch);

  static void GenerateLoadViaGetterForDeopt(MacroAssembler* masm) {
    GenerateLoadViaGetter(masm, Handle<Map>::null(), no_reg, no_reg, -1, -1,
                          no_reg);
  }

  static void GenerateLoadFunctionPrototype(MacroAssembler* masm,
                                            Register receiver,
                                            Register scratch1,
                                            Register scratch2,
                                            Label* miss_label);

  // These constants describe the structure of the interceptor arguments on the
  // stack. The arguments are pushed by the (platform-specific)
  // PushInterceptorArguments and read by LoadPropertyWithInterceptorOnly and
  // LoadWithInterceptor.
  static const int kInterceptorArgsNameIndex = 0;
  static const int kInterceptorArgsThisIndex = 1;
  static const int kInterceptorArgsHolderIndex = 2;
  static const int kInterceptorArgsLength = 3;

 protected:
  virtual Register FrontendHeader(Register object_reg, Handle<Name> name,
                                  Label* miss, ReturnHolder return_what);

  virtual void FrontendFooter(Handle<Name> name, Label* miss);

 private:
  void GenerateLoadCallback(Register reg, Handle<AccessorInfo> callback);

  // Helper emits no code if vector-ics are disabled.
  void InterceptorVectorSlotPush(Register holder_reg);
  enum PopMode { POP, DISCARD };
  void InterceptorVectorSlotPop(Register holder_reg, PopMode mode = POP);

  void GenerateLoadInterceptor(Register holder_reg);
  void GenerateLoadInterceptorWithFollowup(LookupIterator* it,
                                           Register holder_reg);
  void GenerateLoadPostInterceptor(LookupIterator* it, Register reg);

  Register scratch3() { return registers_[4]; }
};


class NamedStoreHandlerCompiler : public PropertyHandlerCompiler {
 public:
  // All store handlers use StoreWithVectorDescriptor calling convention.
  typedef StoreWithVectorDescriptor Descriptor;

  explicit NamedStoreHandlerCompiler(Isolate* isolate, Handle<Map> map,
                                     Handle<JSObject> holder)
      : PropertyHandlerCompiler(isolate, Code::STORE_IC, map, holder,
                                kCacheOnReceiver) {
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
                                  Label* miss, ReturnHolder return_what);

  virtual void FrontendFooter(Handle<Name> name, Label* miss);
  void GenerateRestoreName(Label* label, Handle<Name> name);

 private:
  static Register value();
};


class ElementHandlerCompiler : public PropertyHandlerCompiler {
 public:
  explicit ElementHandlerCompiler(Isolate* isolate)
      : PropertyHandlerCompiler(isolate, Code::KEYED_LOAD_IC,
                                Handle<Map>::null(), Handle<JSObject>::null(),
                                kCacheOnReceiver) {}

  virtual ~ElementHandlerCompiler() {}

  static Handle<Object> GetKeyedLoadHandler(Handle<Map> receiver_map,
                                            Isolate* isolate);
  void CompileElementHandlers(MapHandleList* receiver_maps,
                              List<Handle<Object>>* handlers);
};
}  // namespace internal
}  // namespace v8

#endif  // V8_IC_HANDLER_COMPILER_H_
