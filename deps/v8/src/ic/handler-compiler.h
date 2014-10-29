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

enum PrototypeCheckType { CHECK_ALL_MAPS, SKIP_RECEIVER };

class PropertyHandlerCompiler : public PropertyAccessCompiler {
 public:
  static Handle<Code> Find(Handle<Name> name, Handle<Map> map, Code::Kind kind,
                           CacheHolderFlag cache_holder, Code::StubType type);

 protected:
  PropertyHandlerCompiler(Isolate* isolate, Code::Kind kind,
                          Handle<HeapType> type, Handle<JSObject> holder,
                          CacheHolderFlag cache_holder)
      : PropertyAccessCompiler(isolate, kind, cache_holder),
        type_(type),
        holder_(holder) {}

  virtual ~PropertyHandlerCompiler() {}

  virtual Register FrontendHeader(Register object_reg, Handle<Name> name,
                                  Label* miss) {
    UNREACHABLE();
    return receiver();
  }

  virtual void FrontendFooter(Handle<Name> name, Label* miss) { UNREACHABLE(); }

  Register Frontend(Register object_reg, Handle<Name> name);
  void NonexistentFrontendHeader(Handle<Name> name, Label* miss,
                                 Register scratch1, Register scratch2);

  // TODO(verwaest): Make non-static.
  static void GenerateFastApiCall(MacroAssembler* masm,
                                  const CallOptimization& optimization,
                                  Handle<Map> receiver_map, Register receiver,
                                  Register scratch, bool is_store, int argc,
                                  Register* values);

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
                           PrototypeCheckType check = CHECK_ALL_MAPS);

  Handle<Code> GetCode(Code::Kind kind, Code::StubType type, Handle<Name> name);
  void set_type_for_object(Handle<Object> object);
  void set_holder(Handle<JSObject> holder) { holder_ = holder; }
  Handle<HeapType> type() const { return type_; }
  Handle<JSObject> holder() const { return holder_; }

 private:
  Handle<HeapType> type_;
  Handle<JSObject> holder_;
};


class NamedLoadHandlerCompiler : public PropertyHandlerCompiler {
 public:
  NamedLoadHandlerCompiler(Isolate* isolate, Handle<HeapType> type,
                           Handle<JSObject> holder,
                           CacheHolderFlag cache_holder)
      : PropertyHandlerCompiler(isolate, Code::LOAD_IC, type, holder,
                                cache_holder) {}

  virtual ~NamedLoadHandlerCompiler() {}

  Handle<Code> CompileLoadField(Handle<Name> name, FieldIndex index);

  Handle<Code> CompileLoadCallback(Handle<Name> name,
                                   Handle<ExecutableAccessorInfo> callback);

  Handle<Code> CompileLoadCallback(Handle<Name> name,
                                   const CallOptimization& call_optimization);

  Handle<Code> CompileLoadConstant(Handle<Name> name, int constant_index);

  // The LookupIterator is used to perform a lookup behind the interceptor. If
  // the iterator points to a LookupIterator::PROPERTY, its access will be
  // inlined.
  Handle<Code> CompileLoadInterceptor(LookupIterator* it);

  Handle<Code> CompileLoadViaGetter(Handle<Name> name,
                                    Handle<JSFunction> getter);

  Handle<Code> CompileLoadGlobal(Handle<PropertyCell> cell, Handle<Name> name,
                                 bool is_configurable);

  // Static interface
  static Handle<Code> ComputeLoadNonexistent(Handle<Name> name,
                                             Handle<HeapType> type);

  static void GenerateLoadViaGetter(MacroAssembler* masm, Handle<HeapType> type,
                                    Register receiver,
                                    Handle<JSFunction> getter);

  static void GenerateLoadViaGetterForDeopt(MacroAssembler* masm) {
    GenerateLoadViaGetter(masm, Handle<HeapType>::null(), no_reg,
                          Handle<JSFunction>());
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
  static const int kInterceptorArgsInfoIndex = 1;
  static const int kInterceptorArgsThisIndex = 2;
  static const int kInterceptorArgsHolderIndex = 3;
  static const int kInterceptorArgsLength = 4;

 protected:
  virtual Register FrontendHeader(Register object_reg, Handle<Name> name,
                                  Label* miss);

  virtual void FrontendFooter(Handle<Name> name, Label* miss);

 private:
  Handle<Code> CompileLoadNonexistent(Handle<Name> name);
  void GenerateLoadConstant(Handle<Object> value);
  void GenerateLoadCallback(Register reg,
                            Handle<ExecutableAccessorInfo> callback);
  void GenerateLoadCallback(const CallOptimization& call_optimization,
                            Handle<Map> receiver_map);
  void GenerateLoadInterceptor(Register holder_reg);
  void GenerateLoadInterceptorWithFollowup(LookupIterator* it,
                                           Register holder_reg);
  void GenerateLoadPostInterceptor(LookupIterator* it, Register reg);

  // Generates prototype loading code that uses the objects from the
  // context we were in when this function was called. If the context
  // has changed, a jump to miss is performed. This ties the generated
  // code to a particular context and so must not be used in cases
  // where the generated code is not allowed to have references to
  // objects from a context.
  static void GenerateDirectLoadGlobalFunctionPrototype(MacroAssembler* masm,
                                                        int index,
                                                        Register prototype,
                                                        Label* miss);


  Register scratch4() { return registers_[5]; }
};


class NamedStoreHandlerCompiler : public PropertyHandlerCompiler {
 public:
  explicit NamedStoreHandlerCompiler(Isolate* isolate, Handle<HeapType> type,
                                     Handle<JSObject> holder)
      : PropertyHandlerCompiler(isolate, Code::STORE_IC, type, holder,
                                kCacheOnReceiver) {}

  virtual ~NamedStoreHandlerCompiler() {}

  Handle<Code> CompileStoreTransition(Handle<Map> transition,
                                      Handle<Name> name);
  Handle<Code> CompileStoreField(LookupIterator* it);
  Handle<Code> CompileStoreCallback(Handle<JSObject> object, Handle<Name> name,
                                    Handle<ExecutableAccessorInfo> callback);
  Handle<Code> CompileStoreCallback(Handle<JSObject> object, Handle<Name> name,
                                    const CallOptimization& call_optimization);
  Handle<Code> CompileStoreViaSetter(Handle<JSObject> object, Handle<Name> name,
                                     Handle<JSFunction> setter);
  Handle<Code> CompileStoreInterceptor(Handle<Name> name);

  static void GenerateStoreViaSetter(MacroAssembler* masm,
                                     Handle<HeapType> type, Register receiver,
                                     Handle<JSFunction> setter);

  static void GenerateStoreViaSetterForDeopt(MacroAssembler* masm) {
    GenerateStoreViaSetter(masm, Handle<HeapType>::null(), no_reg,
                           Handle<JSFunction>());
  }

  static void GenerateSlow(MacroAssembler* masm);

 protected:
  virtual Register FrontendHeader(Register object_reg, Handle<Name> name,
                                  Label* miss);

  virtual void FrontendFooter(Handle<Name> name, Label* miss);
  void GenerateRestoreName(Label* label, Handle<Name> name);

 private:
  void GenerateStoreTransition(Handle<Map> transition, Handle<Name> name,
                               Register receiver_reg, Register name_reg,
                               Register value_reg, Register scratch1,
                               Register scratch2, Register scratch3,
                               Label* miss_label, Label* slow);

  void GenerateStoreField(LookupIterator* lookup, Register value_reg,
                          Label* miss_label);

  static Builtins::Name SlowBuiltin(Code::Kind kind) {
    switch (kind) {
      case Code::STORE_IC:
        return Builtins::kStoreIC_Slow;
      case Code::KEYED_STORE_IC:
        return Builtins::kKeyedStoreIC_Slow;
      default:
        UNREACHABLE();
    }
    return Builtins::kStoreIC_Slow;
  }

  static Register value();
};


class ElementHandlerCompiler : public PropertyHandlerCompiler {
 public:
  explicit ElementHandlerCompiler(Isolate* isolate)
      : PropertyHandlerCompiler(isolate, Code::KEYED_LOAD_IC,
                                Handle<HeapType>::null(),
                                Handle<JSObject>::null(), kCacheOnReceiver) {}

  virtual ~ElementHandlerCompiler() {}

  void CompileElementHandlers(MapHandleList* receiver_maps,
                              CodeHandleList* handlers);

  static void GenerateStoreSlow(MacroAssembler* masm);
};
}
}  // namespace v8::internal

#endif  // V8_IC_HANDLER_COMPILER_H_
