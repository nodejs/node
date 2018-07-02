// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERFACE_DESCRIPTORS_H_
#define V8_INTERFACE_DESCRIPTORS_H_

#include <memory>

#include "src/assembler.h"
#include "src/globals.h"
#include "src/isolate.h"
#include "src/macro-assembler.h"

namespace v8 {
namespace internal {

#define INTERFACE_DESCRIPTOR_LIST(V)  \
  V(CppBuiltinAdaptor)                \
  V(CEntry1ArgvOnStack)               \
  V(Allocate)                         \
  V(Void)                             \
  V(ContextOnly)                      \
  V(NoContext)                        \
  V(Load)                             \
  V(LoadWithVector)                   \
  V(LoadGlobal)                       \
  V(LoadGlobalWithVector)             \
  V(Store)                            \
  V(StoreWithVector)                  \
  V(StoreTransition)                  \
  V(StoreGlobal)                      \
  V(StoreGlobalWithVector)            \
  V(FastNewFunctionContext)           \
  V(FastNewObject)                    \
  V(RecordWrite)                      \
  V(TypeConversion)                   \
  V(TypeConversionStackParameter)     \
  V(Typeof)                           \
  V(CallFunction)                     \
  V(CallVarargs)                      \
  V(CallForwardVarargs)               \
  V(CallWithSpread)                   \
  V(CallWithArrayLike)                \
  V(CallTrampoline)                   \
  V(ConstructStub)                    \
  V(ConstructVarargs)                 \
  V(ConstructForwardVarargs)          \
  V(ConstructWithSpread)              \
  V(ConstructWithArrayLike)           \
  V(JSTrampoline)                     \
  V(Abort)                            \
  V(AllocateHeapNumber)               \
  V(ArrayConstructor)                 \
  V(ArrayNoArgumentConstructor)       \
  V(ArraySingleArgumentConstructor)   \
  V(ArrayNArgumentsConstructor)       \
  V(Compare)                          \
  V(BinaryOp)                         \
  V(StringAt)                         \
  V(StringSubstring)                  \
  V(GetProperty)                      \
  V(ArgumentAdaptor)                  \
  V(ApiCallback)                      \
  V(ApiGetter)                        \
  V(GrowArrayElements)                \
  V(NewArgumentsElements)             \
  V(InterpreterDispatch)              \
  V(InterpreterPushArgsThenCall)      \
  V(InterpreterPushArgsThenConstruct) \
  V(InterpreterCEntry1)               \
  V(InterpreterCEntry2)               \
  V(ResumeGenerator)                  \
  V(FrameDropperTrampoline)           \
  V(RunMicrotasks)                    \
  V(WasmGrowMemory)                   \
  BUILTIN_LIST_TFS(V)

class V8_EXPORT_PRIVATE CallInterfaceDescriptorData {
 public:
  enum Flag {
    kNoFlags = 0u,
    kNoContext = 1u << 0,
  };
  typedef base::Flags<Flag> Flags;

  CallInterfaceDescriptorData() = default;

  // A copy of the passed in registers and param_representations is made
  // and owned by the CallInterfaceDescriptorData.

  void InitializePlatformSpecific(int register_parameter_count,
                                  const Register* registers);

  // if machine_types is null, then an array of size
  // (return_count + parameter_count) will be created with
  // MachineType::AnyTagged() for each member.
  //
  // if machine_types is not null, then it should be of the size
  // (return_count + parameter_count). Those members of the parameter array will
  // be initialized from {machine_types}, and the rest initialized to
  // MachineType::AnyTagged().
  void InitializePlatformIndependent(Flags flags, int return_count,
                                     int parameter_count,
                                     const MachineType* machine_types,
                                     int machine_types_length);

  void Reset();

  bool IsInitialized() const {
    return register_param_count_ >= 0 && return_count_ >= 0 &&
           param_count_ >= 0;
  }

  Flags flags() const { return flags_; }
  int return_count() const { return return_count_; }
  int param_count() const { return param_count_; }
  int register_param_count() const { return register_param_count_; }
  Register register_param(int index) const { return register_params_[index]; }
  Register* register_params() const { return register_params_; }
  MachineType return_type(int index) const {
    DCHECK_LT(index, return_count_);
    return machine_types_[index];
  }
  MachineType param_type(int index) const {
    DCHECK_LT(index, param_count_);
    return machine_types_[return_count_ + index];
  }

  void RestrictAllocatableRegisters(const Register* registers, int num) {
    DCHECK_EQ(allocatable_registers_, 0);
    for (int i = 0; i < num; ++i) {
      allocatable_registers_ |= registers[i].bit();
    }
    DCHECK_GT(NumRegs(allocatable_registers_), 0);
  }

  RegList allocatable_registers() const { return allocatable_registers_; }

 private:
  int register_param_count_ = -1;
  int return_count_ = -1;
  int param_count_ = -1;
  Flags flags_ = kNoFlags;

  // Specifying the set of registers that could be used by the register
  // allocator. Currently, it's only used by RecordWrite code stub.
  RegList allocatable_registers_ = 0;

  // |registers_params_| defines registers that are used for parameter passing.
  // |machine_types_| defines machine types for resulting values and incomping
  // parameters.
  // Both arrays are allocated dynamically by the InterfaceDescriptor and
  // freed on destruction. This is because static arrays cause creation of
  // runtime static initializers which we don't want.
  Register* register_params_ = nullptr;
  MachineType* machine_types_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CallInterfaceDescriptorData);
};

class V8_EXPORT_PRIVATE CallDescriptors : public AllStatic {
 public:
  enum Key {
#define DEF_ENUM(name, ...) name,
    INTERFACE_DESCRIPTOR_LIST(DEF_ENUM)
#undef DEF_ENUM
        NUMBER_OF_DESCRIPTORS
  };

  static void InitializeOncePerProcess();
  static void TearDown();

  static CallInterfaceDescriptorData* call_descriptor_data(
      CallDescriptors::Key key) {
    return &call_descriptor_data_[key];
  }

  static Key GetKey(const CallInterfaceDescriptorData* data) {
    ptrdiff_t index = data - call_descriptor_data_;
    DCHECK_LE(0, index);
    DCHECK_LT(index, CallDescriptors::NUMBER_OF_DESCRIPTORS);
    return static_cast<CallDescriptors::Key>(index);
  }

 private:
  static CallInterfaceDescriptorData
      call_descriptor_data_[NUMBER_OF_DESCRIPTORS];
};

class V8_EXPORT_PRIVATE CallInterfaceDescriptor {
 public:
  typedef CallInterfaceDescriptorData::Flags Flags;

  CallInterfaceDescriptor() : data_(nullptr) {}
  virtual ~CallInterfaceDescriptor() {}

  CallInterfaceDescriptor(CallDescriptors::Key key)
      : data_(CallDescriptors::call_descriptor_data(key)) {}

  Flags flags() const { return data()->flags(); }

  bool HasContextParameter() const {
    return (flags() & CallInterfaceDescriptorData::kNoContext) == 0;
  }

  int GetReturnCount() const { return data()->return_count(); }

  MachineType GetReturnType(int index) const {
    DCHECK_LT(index, data()->return_count());
    return data()->return_type(index);
  }

  int GetParameterCount() const { return data()->param_count(); }

  int GetRegisterParameterCount() const {
    return data()->register_param_count();
  }

  int GetStackParameterCount() const {
    return data()->param_count() - data()->register_param_count();
  }

  Register GetRegisterParameter(int index) const {
    return data()->register_param(index);
  }

  MachineType GetParameterType(int index) const {
    DCHECK_LT(index, data()->param_count());
    return data()->param_type(index);
  }

  RegList allocatable_registers() const {
    return data()->allocatable_registers();
  }

  static const Register ContextRegister();

  const char* DebugName() const;

 protected:
  const CallInterfaceDescriptorData* data() const { return data_; }

  virtual void InitializePlatformSpecific(CallInterfaceDescriptorData* data) {
    UNREACHABLE();
  }

  virtual void InitializePlatformIndependent(
      CallInterfaceDescriptorData* data) {
    // Default descriptor configuration: one result, all parameters are passed
    // in registers and all parameters have MachineType::AnyTagged() type.
    data->InitializePlatformIndependent(CallInterfaceDescriptorData::kNoFlags,
                                        1, data->register_param_count(),
                                        nullptr, 0);
  }

  // Initializes |data| using the platform dependent default set of registers.
  // It is intended to be used for TurboFan stubs when particular set of
  // registers does not matter.
  static void DefaultInitializePlatformSpecific(
      CallInterfaceDescriptorData* data, int register_parameter_count);

  // Initializes |data| using the platform dependent default set of registers
  // for JavaScript-compatible calling convention.
  // It is intended to be used for TurboFan stubs being called with JavaScript
  // linkage + additional parameters on registers and stack.
  static void JSDefaultInitializePlatformSpecific(
      CallInterfaceDescriptorData* data, int non_js_register_parameter_count);

 private:
  // {CallDescriptors} is allowed to call the private {Initialize} method.
  friend class CallDescriptors;

  const CallInterfaceDescriptorData* data_;

  void Initialize(CallInterfaceDescriptorData* data) {
    // The passed pointer should be a modifiable pointer to our own data.
    DCHECK_EQ(data, data_);
    DCHECK(!data->IsInitialized());
    InitializePlatformSpecific(data);
    InitializePlatformIndependent(data);
    DCHECK(data->IsInitialized());
  }
};

#define DECLARE_DESCRIPTOR_WITH_BASE(name, base) \
 public:                                         \
  explicit name() : base(key()) {}               \
  static inline CallDescriptors::Key key();

constexpr int kMaxBuiltinRegisterParams = 5;

#define DECLARE_DEFAULT_DESCRIPTOR(name, base)                                 \
  DECLARE_DESCRIPTOR_WITH_BASE(name, base)                                     \
 protected:                                                                    \
  static const int kRegisterParams =                                           \
      kParameterCount > kMaxBuiltinRegisterParams ? kMaxBuiltinRegisterParams  \
                                                  : kParameterCount;           \
  static const int kStackParams = kParameterCount - kRegisterParams;           \
  void InitializePlatformSpecific(CallInterfaceDescriptorData* data)           \
      override {                                                               \
    DefaultInitializePlatformSpecific(data, kRegisterParams);                  \
  }                                                                            \
  void InitializePlatformIndependent(CallInterfaceDescriptorData* data)        \
      override {                                                               \
    data->InitializePlatformIndependent(Flags(kDescriptorFlags), kReturnCount, \
                                        kParameterCount, nullptr, 0);          \
  }                                                                            \
  name(CallDescriptors::Key key) : base(key) {}                                \
                                                                               \
 public:

#define DECLARE_JS_COMPATIBLE_DESCRIPTOR(name, base,                        \
                                         non_js_reg_parameters_count)       \
  DECLARE_DESCRIPTOR_WITH_BASE(name, base)                                  \
 protected:                                                                 \
  void InitializePlatformSpecific(CallInterfaceDescriptorData* data)        \
      override {                                                            \
    JSDefaultInitializePlatformSpecific(data, non_js_reg_parameters_count); \
  }                                                                         \
  name(CallDescriptors::Key key) : base(key) {}                             \
                                                                            \
 public:

#define DEFINE_RESULT_AND_PARAMETERS(return_count, ...)   \
  static constexpr int kDescriptorFlags =                 \
      CallInterfaceDescriptorData::kNoFlags;              \
  static constexpr int kReturnCount = return_count;       \
  enum ParameterIndices {                                 \
    __dummy = -1, /* to be able to pass zero arguments */ \
    ##__VA_ARGS__,                                        \
                                                          \
    kParameterCount,                                      \
    kContext = kParameterCount /* implicit parameter */   \
  };

#define DEFINE_RESULT_AND_PARAMETERS_NO_CONTEXT(return_count, ...) \
  static constexpr int kDescriptorFlags =                          \
      CallInterfaceDescriptorData::kNoContext;                     \
  static constexpr int kReturnCount = return_count;                \
  enum ParameterIndices {                                          \
    __dummy = -1, /* to be able to pass zero arguments */          \
    ##__VA_ARGS__,                                                 \
                                                                   \
    kParameterCount                                                \
  };

#define DEFINE_PARAMETERS(...) DEFINE_RESULT_AND_PARAMETERS(1, ##__VA_ARGS__)

#define DEFINE_PARAMETERS_NO_CONTEXT(...) \
  DEFINE_RESULT_AND_PARAMETERS_NO_CONTEXT(1, ##__VA_ARGS__)

#define DEFINE_RESULT_AND_PARAMETER_TYPES(...)                                 \
  void InitializePlatformIndependent(CallInterfaceDescriptorData* data)        \
      override {                                                               \
    MachineType machine_types[] = {__VA_ARGS__};                               \
    static_assert(                                                             \
        kReturnCount + kParameterCount == arraysize(machine_types),            \
        "Parameter names definition is not consistent with parameter types");  \
    data->InitializePlatformIndependent(Flags(kDescriptorFlags), kReturnCount, \
                                        kParameterCount, machine_types,        \
                                        arraysize(machine_types));             \
  }

#define DEFINE_PARAMETER_TYPES(...)                                        \
  DEFINE_RESULT_AND_PARAMETER_TYPES(MachineType::AnyTagged() /* result */, \
                                    ##__VA_ARGS__)

#define DEFINE_JS_PARAMETERS(...)                       \
  static constexpr int kDescriptorFlags =               \
      CallInterfaceDescriptorData::kNoFlags;            \
  static constexpr int kReturnCount = 1;                \
  enum ParameterIndices {                               \
    kTarget,                                            \
    kNewTarget,                                         \
    kActualArgumentsCount,                              \
    ##__VA_ARGS__,                                      \
                                                        \
    kParameterCount,                                    \
    kContext = kParameterCount /* implicit parameter */ \
  };

#define DEFINE_JS_PARAMETER_TYPES(...)                                         \
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(), /* kTarget */               \
                         MachineType::AnyTagged(), /* kNewTarget */            \
                         MachineType::Int32(),     /* kActualArgumentsCount */ \
                         ##__VA_ARGS__)

#define DECLARE_DESCRIPTOR(name, base)                                         \
  DECLARE_DESCRIPTOR_WITH_BASE(name, base)                                     \
 protected:                                                                    \
  void InitializePlatformSpecific(CallInterfaceDescriptorData* data) override; \
  name(CallDescriptors::Key key) : base(key) {}                                \
                                                                               \
 public:

class V8_EXPORT_PRIVATE VoidDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS()
  DEFINE_PARAMETER_TYPES()
  DECLARE_DESCRIPTOR(VoidDescriptor, CallInterfaceDescriptor)
};

class AllocateDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kRequestedSize)
  DEFINE_RESULT_AND_PARAMETER_TYPES(MachineType::TaggedPointer(),  // result 1
                                    MachineType::Int32())  // kRequestedSize
  DECLARE_DESCRIPTOR(AllocateDescriptor, CallInterfaceDescriptor)
};

// This descriptor defines the JavaScript calling convention that can be used
// by stubs: target, new.target, argc (not including the receiver) and context
// are passed in registers while receiver and the rest of the JS arguments are
// passed on the stack.
class JSTrampolineDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_JS_PARAMETERS()
  DEFINE_JS_PARAMETER_TYPES()

  DECLARE_JS_COMPATIBLE_DESCRIPTOR(JSTrampolineDescriptor,
                                   CallInterfaceDescriptor, 0)
};

class ContextOnlyDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS()
  DEFINE_PARAMETER_TYPES()
  DECLARE_DESCRIPTOR(ContextOnlyDescriptor, CallInterfaceDescriptor)
};

class NoContextDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT()
  DEFINE_PARAMETER_TYPES()
  DECLARE_DESCRIPTOR(NoContextDescriptor, CallInterfaceDescriptor)
};

// LoadDescriptor is used by all stubs that implement Load/KeyedLoad ICs.
class LoadDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kReceiver, kName, kSlot)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),     // kReceiver
                         MachineType::AnyTagged(),     // kName
                         MachineType::TaggedSigned())  // kSlot
  DECLARE_DESCRIPTOR(LoadDescriptor, CallInterfaceDescriptor)

  static const Register ReceiverRegister();
  static const Register NameRegister();
  static const Register SlotRegister();
};

class LoadGlobalDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kName, kSlot)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),     // kName
                         MachineType::TaggedSigned())  // kSlot
  DECLARE_DESCRIPTOR(LoadGlobalDescriptor, CallInterfaceDescriptor)

  static const Register NameRegister() {
    return LoadDescriptor::NameRegister();
  }

  static const Register SlotRegister() {
    return LoadDescriptor::SlotRegister();
  }
};

class StoreDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kReceiver, kName, kValue, kSlot)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),     // kReceiver
                         MachineType::AnyTagged(),     // kName
                         MachineType::AnyTagged(),     // kValue
                         MachineType::TaggedSigned())  // kSlot
  DECLARE_DESCRIPTOR(StoreDescriptor, CallInterfaceDescriptor)

  static const Register ReceiverRegister();
  static const Register NameRegister();
  static const Register ValueRegister();
  static const Register SlotRegister();

#if V8_TARGET_ARCH_IA32
  static const bool kPassLastArgsOnStack = true;
#else
  static const bool kPassLastArgsOnStack = false;
#endif

  // Pass value and slot through the stack.
  static const int kStackArgumentsCount = kPassLastArgsOnStack ? 2 : 0;
};

class StoreTransitionDescriptor : public StoreDescriptor {
 public:
  DEFINE_PARAMETERS(kReceiver, kName, kMap, kValue, kSlot, kVector)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),     // kReceiver
                         MachineType::AnyTagged(),     // kName
                         MachineType::AnyTagged(),     // kMap
                         MachineType::AnyTagged(),     // kValue
                         MachineType::TaggedSigned(),  // kSlot
                         MachineType::AnyTagged())     // kVector
  DECLARE_DESCRIPTOR(StoreTransitionDescriptor, StoreDescriptor)

  static const Register MapRegister();
  static const Register SlotRegister();
  static const Register VectorRegister();

  // Pass value, slot and vector through the stack.
  static const int kStackArgumentsCount = kPassLastArgsOnStack ? 3 : 0;
};

class StoreWithVectorDescriptor : public StoreDescriptor {
 public:
  DEFINE_PARAMETERS(kReceiver, kName, kValue, kSlot, kVector)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),     // kReceiver
                         MachineType::AnyTagged(),     // kName
                         MachineType::AnyTagged(),     // kValue
                         MachineType::TaggedSigned(),  // kSlot
                         MachineType::AnyTagged())     // kVector
  DECLARE_DESCRIPTOR(StoreWithVectorDescriptor, StoreDescriptor)

  static const Register VectorRegister();

  // Pass value, slot and vector through the stack.
  static const int kStackArgumentsCount = kPassLastArgsOnStack ? 3 : 0;
};

class StoreGlobalDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kName, kValue, kSlot)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),     // kName
                         MachineType::AnyTagged(),     // kValue
                         MachineType::TaggedSigned())  // kSlot
  DECLARE_DESCRIPTOR(StoreGlobalDescriptor, CallInterfaceDescriptor)

  static const bool kPassLastArgsOnStack =
      StoreDescriptor::kPassLastArgsOnStack;
  // Pass value and slot through the stack.
  static const int kStackArgumentsCount = kPassLastArgsOnStack ? 2 : 0;

  static const Register NameRegister() {
    return StoreDescriptor::NameRegister();
  }

  static const Register ValueRegister() {
    return StoreDescriptor::ValueRegister();
  }

  static const Register SlotRegister() {
    return StoreDescriptor::SlotRegister();
  }
};

class StoreGlobalWithVectorDescriptor : public StoreGlobalDescriptor {
 public:
  DEFINE_PARAMETERS(kName, kValue, kSlot, kVector)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),     // kName
                         MachineType::AnyTagged(),     // kValue
                         MachineType::TaggedSigned(),  // kSlot
                         MachineType::AnyTagged())     // kVector
  DECLARE_DESCRIPTOR(StoreGlobalWithVectorDescriptor, StoreGlobalDescriptor)

  static const Register VectorRegister() {
    return StoreWithVectorDescriptor::VectorRegister();
  }

  // Pass value, slot and vector through the stack.
  static const int kStackArgumentsCount = kPassLastArgsOnStack ? 3 : 0;
};

class LoadWithVectorDescriptor : public LoadDescriptor {
 public:
  DEFINE_PARAMETERS(kReceiver, kName, kSlot, kVector)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),     // kReceiver
                         MachineType::AnyTagged(),     // kName
                         MachineType::TaggedSigned(),  // kSlot
                         MachineType::AnyTagged())     // kVector
  DECLARE_DESCRIPTOR(LoadWithVectorDescriptor, LoadDescriptor)

  static const Register VectorRegister();
};

class LoadGlobalWithVectorDescriptor : public LoadGlobalDescriptor {
 public:
  DEFINE_PARAMETERS(kName, kSlot, kVector)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),     // kName
                         MachineType::TaggedSigned(),  // kSlot
                         MachineType::AnyTagged())     // kVector
  DECLARE_DESCRIPTOR(LoadGlobalWithVectorDescriptor, LoadGlobalDescriptor)

  static const Register VectorRegister() {
    return LoadWithVectorDescriptor::VectorRegister();
  }
};

class FastNewFunctionContextDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kScopeInfo, kSlots)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kScopeInfo
                         MachineType::Int32())      // kSlots
  DECLARE_DESCRIPTOR(FastNewFunctionContextDescriptor, CallInterfaceDescriptor)

  static const Register ScopeInfoRegister();
  static const Register SlotsRegister();
};

class FastNewObjectDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kTarget, kNewTarget)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kTarget
                         MachineType::AnyTagged())  // kNewTarget
  DECLARE_DESCRIPTOR(FastNewObjectDescriptor, CallInterfaceDescriptor)
  static const Register TargetRegister();
  static const Register NewTargetRegister();
};

class RecordWriteDescriptor final : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kObject, kSlot, kIsolate, kRememberedSet, kFPMode)
  DEFINE_PARAMETER_TYPES(MachineType::TaggedPointer(),  // kObject
                         MachineType::Pointer(),        // kSlot
                         MachineType::Pointer(),        // kIsolate
                         MachineType::TaggedSigned(),   // kRememberedSet
                         MachineType::TaggedSigned())   // kFPMode

  DECLARE_DESCRIPTOR(RecordWriteDescriptor, CallInterfaceDescriptor)
};

class TypeConversionDescriptor final : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kArgument)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged())
  DECLARE_DESCRIPTOR(TypeConversionDescriptor, CallInterfaceDescriptor)

  static const Register ArgumentRegister();
};

class TypeConversionStackParameterDescriptor final
    : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kArgument)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged())
  DECLARE_DESCRIPTOR(TypeConversionStackParameterDescriptor,
                     CallInterfaceDescriptor)
};

class GetPropertyDescriptor final : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kObject, kKey)
  DECLARE_DEFAULT_DESCRIPTOR(GetPropertyDescriptor, CallInterfaceDescriptor)
};

class TypeofDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kObject)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged())
  DECLARE_DESCRIPTOR(TypeofDescriptor, CallInterfaceDescriptor)
};

class CallTrampolineDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kFunction, kActualArgumentsCount)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kFunction
                         MachineType::Int32())      // kActualArgumentsCount
  DECLARE_DESCRIPTOR(CallTrampolineDescriptor, CallInterfaceDescriptor)
};

class CallVarargsDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kTarget, kActualArgumentsCount, kArgumentsList,
                    kArgumentsLength)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kTarget
                         MachineType::Int32(),      // kActualArgumentsCount
                         MachineType::AnyTagged(),  // kArgumentsList
                         MachineType::Int32())      // kArgumentsLength
  DECLARE_DESCRIPTOR(CallVarargsDescriptor, CallInterfaceDescriptor)
};

class CallForwardVarargsDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kTarget, kActualArgumentsCount, kStartIndex)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kTarget
                         MachineType::Int32(),      // kActualArgumentsCount
                         MachineType::Int32())      // kStartIndex
  DECLARE_DESCRIPTOR(CallForwardVarargsDescriptor, CallInterfaceDescriptor)
};

class CallWithSpreadDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kTarget, kArgumentsCount, kSpread)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kTarget
                         MachineType::Int32(),      // kArgumentsCount
                         MachineType::AnyTagged())  // kSpread
  DECLARE_DESCRIPTOR(CallWithSpreadDescriptor, CallInterfaceDescriptor)
};

class CallWithArrayLikeDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kTarget, kArgumentsList)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kTarget
                         MachineType::AnyTagged())  // kArgumentsList
  DECLARE_DESCRIPTOR(CallWithArrayLikeDescriptor, CallInterfaceDescriptor)
};

class ConstructVarargsDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_JS_PARAMETERS(kArgumentsList, kArgumentsLength)
  DEFINE_JS_PARAMETER_TYPES(MachineType::AnyTagged(),  // kArgumentsList
                            MachineType::Int32())      // kArgumentsLength
  DECLARE_DESCRIPTOR(ConstructVarargsDescriptor, CallInterfaceDescriptor)
};

class ConstructForwardVarargsDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_JS_PARAMETERS(kStartIndex)
  DEFINE_JS_PARAMETER_TYPES(MachineType::Int32())
  DECLARE_DESCRIPTOR(ConstructForwardVarargsDescriptor, CallInterfaceDescriptor)
};

class ConstructWithSpreadDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_JS_PARAMETERS(kSpread)
  DEFINE_JS_PARAMETER_TYPES(MachineType::AnyTagged())
  DECLARE_DESCRIPTOR(ConstructWithSpreadDescriptor, CallInterfaceDescriptor)
};

class ConstructWithArrayLikeDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kTarget, kNewTarget, kArgumentsList)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kTarget
                         MachineType::AnyTagged(),  // kNewTarget
                         MachineType::AnyTagged())  // kArgumentsList
  DECLARE_DESCRIPTOR(ConstructWithArrayLikeDescriptor, CallInterfaceDescriptor)
};

// TODO(ishell): consider merging this with ArrayConstructorDescriptor
class ConstructStubDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_JS_PARAMETERS(kAllocationSite)
  DEFINE_JS_PARAMETER_TYPES(MachineType::AnyTagged());

  // TODO(ishell): Use DECLARE_JS_COMPATIBLE_DESCRIPTOR if registers match
  DECLARE_DESCRIPTOR(ConstructStubDescriptor, CallInterfaceDescriptor)
};

class CallFunctionDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kTarget)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged())
  DECLARE_DESCRIPTOR(CallFunctionDescriptor, CallInterfaceDescriptor)
};

class AbortDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kMessageOrMessageId)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged())
  DECLARE_DESCRIPTOR(AbortDescriptor, CallInterfaceDescriptor)
};

class AllocateHeapNumberDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT()
  DEFINE_PARAMETER_TYPES()
  DECLARE_DESCRIPTOR(AllocateHeapNumberDescriptor, CallInterfaceDescriptor)
};

class ArrayConstructorDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_JS_PARAMETERS(kAllocationSite)
  DEFINE_JS_PARAMETER_TYPES(MachineType::AnyTagged());

  DECLARE_JS_COMPATIBLE_DESCRIPTOR(ArrayConstructorDescriptor,
                                   CallInterfaceDescriptor, 1)
};

class ArrayNArgumentsConstructorDescriptor : public CallInterfaceDescriptor {
 public:
  // This descriptor declares only register arguments while respective number
  // of JS arguments stay on the expression stack.
  // The ArrayNArgumentsConstructor builtin does not access stack arguments
  // directly it just forwards them to the runtime function.
  DEFINE_PARAMETERS(kFunction, kAllocationSite, kActualArgumentsCount)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kFunction,
                         MachineType::AnyTagged(),  // kAllocationSite
                         MachineType::Int32())      // kActualArgumentsCount
  DECLARE_DESCRIPTOR(ArrayNArgumentsConstructorDescriptor,
                     CallInterfaceDescriptor)
};

class ArrayNoArgumentConstructorDescriptor
    : public ArrayNArgumentsConstructorDescriptor {
 public:
  // This descriptor declares same register arguments as the parent
  // ArrayNArgumentsConstructorDescriptor and it declares indices for
  // JS arguments passed on the expression stack.
  DEFINE_PARAMETERS(kFunction, kAllocationSite, kActualArgumentsCount,
                    kFunctionParameter)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kFunction
                         MachineType::AnyTagged(),  // kAllocationSite
                         MachineType::Int32(),      // kActualArgumentsCount
                         MachineType::AnyTagged())  // kFunctionParameter
  DECLARE_DESCRIPTOR(ArrayNoArgumentConstructorDescriptor,
                     ArrayNArgumentsConstructorDescriptor)
};

class ArraySingleArgumentConstructorDescriptor
    : public ArrayNArgumentsConstructorDescriptor {
 public:
  // This descriptor declares same register arguments as the parent
  // ArrayNArgumentsConstructorDescriptor and it declares indices for
  // JS arguments passed on the expression stack.
  DEFINE_PARAMETERS(kFunction, kAllocationSite, kActualArgumentsCount,
                    kFunctionParameter, kArraySizeSmiParameter)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kFunction
                         MachineType::AnyTagged(),  // kAllocationSite
                         MachineType::Int32(),      // kActualArgumentsCount
                         MachineType::AnyTagged(),  // kFunctionParameter
                         MachineType::AnyTagged())  // kArraySizeSmiParameter
  DECLARE_DESCRIPTOR(ArraySingleArgumentConstructorDescriptor,
                     ArrayNArgumentsConstructorDescriptor)
};

class CompareDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kLeft, kRight)
  DECLARE_DESCRIPTOR(CompareDescriptor, CallInterfaceDescriptor)
};


class BinaryOpDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kLeft, kRight)
  DECLARE_DESCRIPTOR(BinaryOpDescriptor, CallInterfaceDescriptor)
};

// This desciptor is shared among String.p.charAt/charCodeAt/codePointAt
// as they all have the same interface.
class StringAtDescriptor final : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kReceiver, kPosition)
  // TODO(turbofan): Return untagged value here.
  DEFINE_RESULT_AND_PARAMETER_TYPES(MachineType::TaggedSigned(),  // result 1
                                    MachineType::AnyTagged(),     // kReceiver
                                    MachineType::IntPtr())        // kPosition
  DECLARE_DESCRIPTOR(StringAtDescriptor, CallInterfaceDescriptor)
};

class StringSubstringDescriptor final : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kString, kFrom, kTo)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kString
                         MachineType::IntPtr(),     // kFrom
                         MachineType::IntPtr())     // kTo

  // TODO(turbofan): Allow builtins to return untagged values.
  DECLARE_DESCRIPTOR(StringSubstringDescriptor, CallInterfaceDescriptor)
};

class ArgumentAdaptorDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_JS_PARAMETERS(kExpectedArgumentsCount)
  DEFINE_JS_PARAMETER_TYPES(MachineType::Int32())
  DECLARE_DESCRIPTOR(ArgumentAdaptorDescriptor, CallInterfaceDescriptor)
};

class CppBuiltinAdaptorDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_JS_PARAMETERS(kCFunction)
  DEFINE_JS_PARAMETER_TYPES(MachineType::Pointer())
  DECLARE_JS_COMPATIBLE_DESCRIPTOR(CppBuiltinAdaptorDescriptor,
                                   CallInterfaceDescriptor, 1)
};

class CEntry1ArgvOnStackDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kArity,          // register argument
                    kCFunction,      // register argument
                    kPadding,        // stack argument 1 (just padding)
                    kArgcSmi,        // stack argument 2
                    kTargetCopy,     // stack argument 3
                    kNewTargetCopy)  // stack argument 4
  DEFINE_PARAMETER_TYPES(MachineType::Int32(),      // kArity
                         MachineType::Pointer(),    // kCFunction
                         MachineType::AnyTagged(),  // kPadding
                         MachineType::AnyTagged(),  // kArgcSmi
                         MachineType::AnyTagged(),  // kTargetCopy
                         MachineType::AnyTagged())  // kNewTargetCopy
  DECLARE_DESCRIPTOR(CEntry1ArgvOnStackDescriptor, CallInterfaceDescriptor)
};

class ApiCallbackDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kTargetContext, kCallData, kHolder,
                               kApiFunctionAddress)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kTargetContext
                         MachineType::AnyTagged(),  // kCallData
                         MachineType::AnyTagged(),  // kHolder
                         MachineType::Pointer())    // kApiFunctionAddress
  DECLARE_DESCRIPTOR(ApiCallbackDescriptor, CallInterfaceDescriptor)
};

class ApiGetterDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kReceiver, kHolder, kCallback)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kReceiver
                         MachineType::AnyTagged(),  // kHolder
                         MachineType::AnyTagged())  // kCallback
  DECLARE_DESCRIPTOR(ApiGetterDescriptor, CallInterfaceDescriptor)

  static const Register ReceiverRegister();
  static const Register HolderRegister();
  static const Register CallbackRegister();
};

// TODO(turbofan): We should probably rename this to GrowFastElementsDescriptor.
class GrowArrayElementsDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kObject, kKey)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kObject
                         MachineType::AnyTagged())  // kKey
  DECLARE_DESCRIPTOR(GrowArrayElementsDescriptor, CallInterfaceDescriptor)

  static const Register ObjectRegister();
  static const Register KeyRegister();
};

class NewArgumentsElementsDescriptor final : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kFrame, kLength, kMappedCount)
  DEFINE_PARAMETER_TYPES(MachineType::Pointer(),       // kFrame
                         MachineType::TaggedSigned(),  // kLength
                         MachineType::TaggedSigned())  // kMappedCount
  DECLARE_DESCRIPTOR(NewArgumentsElementsDescriptor, CallInterfaceDescriptor)
};

class V8_EXPORT_PRIVATE InterpreterDispatchDescriptor
    : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kAccumulator, kBytecodeOffset, kBytecodeArray,
                    kDispatchTable)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kAccumulator
                         MachineType::IntPtr(),     // kBytecodeOffset
                         MachineType::AnyTagged(),  // kBytecodeArray
                         MachineType::IntPtr())     // kDispatchTable
  DECLARE_DESCRIPTOR(InterpreterDispatchDescriptor, CallInterfaceDescriptor)
};

class InterpreterPushArgsThenCallDescriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kNumberOfArguments, kFirstArgument, kFunction)
  DEFINE_PARAMETER_TYPES(MachineType::Int32(),      // kNumberOfArguments
                         MachineType::Pointer(),    // kFirstArgument
                         MachineType::AnyTagged())  // kFunction
  DECLARE_DESCRIPTOR(InterpreterPushArgsThenCallDescriptor,
                     CallInterfaceDescriptor)
};

class InterpreterPushArgsThenConstructDescriptor
    : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kNumberOfArguments, kNewTarget, kConstructor,
                    kFeedbackElement, kFirstArgument)
  DEFINE_PARAMETER_TYPES(MachineType::Int32(),      // kNumberOfArguments
                         MachineType::AnyTagged(),  // kNewTarget
                         MachineType::AnyTagged(),  // kConstructor
                         MachineType::AnyTagged(),  // kFeedbackElement
                         MachineType::Pointer())    // kFirstArgument
  DECLARE_DESCRIPTOR(InterpreterPushArgsThenConstructDescriptor,
                     CallInterfaceDescriptor)
};

class InterpreterCEntry1Descriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_RESULT_AND_PARAMETERS(1, kNumberOfArguments, kFirstArgument,
                               kFunctionEntry)
  DEFINE_RESULT_AND_PARAMETER_TYPES(MachineType::AnyTagged(),  // result 1
                                    MachineType::Int32(),  // kNumberOfArguments
                                    MachineType::Pointer(),  // kFirstArgument
                                    MachineType::Pointer())  // kFunctionEntry
  DECLARE_DESCRIPTOR(InterpreterCEntry1Descriptor, CallInterfaceDescriptor)
};

class InterpreterCEntry2Descriptor : public CallInterfaceDescriptor {
 public:
  DEFINE_RESULT_AND_PARAMETERS(2, kNumberOfArguments, kFirstArgument,
                               kFunctionEntry)
  DEFINE_RESULT_AND_PARAMETER_TYPES(MachineType::AnyTagged(),  // result 1
                                    MachineType::AnyTagged(),  // result 2
                                    MachineType::Int32(),  // kNumberOfArguments
                                    MachineType::Pointer(),  // kFirstArgument
                                    MachineType::Pointer())  // kFunctionEntry
  DECLARE_DESCRIPTOR(InterpreterCEntry2Descriptor, CallInterfaceDescriptor)
};

class ResumeGeneratorDescriptor final : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kValue, kGenerator)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kValue
                         MachineType::AnyTagged())  // kGenerator
  DECLARE_DESCRIPTOR(ResumeGeneratorDescriptor, CallInterfaceDescriptor)
};

class FrameDropperTrampolineDescriptor final : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS(kRestartFp)
  DEFINE_PARAMETER_TYPES(MachineType::Pointer())
  DECLARE_DESCRIPTOR(FrameDropperTrampolineDescriptor, CallInterfaceDescriptor)
};

class RunMicrotasksDescriptor final : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS()
  DECLARE_DEFAULT_DESCRIPTOR(RunMicrotasksDescriptor, CallInterfaceDescriptor)
};

class WasmGrowMemoryDescriptor final : public CallInterfaceDescriptor {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kNumPages)
  DEFINE_RESULT_AND_PARAMETER_TYPES(MachineType::Int32(),  // result 1
                                    MachineType::Int32())  // kNumPages
  DECLARE_DESCRIPTOR(WasmGrowMemoryDescriptor, CallInterfaceDescriptor)
};

#define DEFINE_TFS_BUILTIN_DESCRIPTOR(Name, ...)                          \
  class Name##Descriptor : public CallInterfaceDescriptor {               \
   public:                                                                \
    DEFINE_PARAMETERS(__VA_ARGS__)                                        \
    DECLARE_DEFAULT_DESCRIPTOR(Name##Descriptor, CallInterfaceDescriptor) \
  };
BUILTIN_LIST_TFS(DEFINE_TFS_BUILTIN_DESCRIPTOR)
#undef DEFINE_TFS_BUILTIN_DESCRIPTOR

#undef DECLARE_DEFAULT_DESCRIPTOR
#undef DECLARE_DESCRIPTOR_WITH_BASE
#undef DECLARE_DESCRIPTOR
#undef DECLARE_JS_COMPATIBLE_DESCRIPTOR
#undef DEFINE_RESULT_AND_PARAMETERS
#undef DEFINE_RESULT_AND_PARAMETERS_NO_CONTEXT
#undef DEFINE_PARAMETERS
#undef DEFINE_PARAMETERS_NO_CONTEXT
#undef DEFINE_RESULT_AND_PARAMETER_TYPES
#undef DEFINE_PARAMETER_TYPES
#undef DEFINE_JS_PARAMETERS
#undef DEFINE_JS_PARAMETER_TYPES

// We define the association between CallDescriptors::Key and the specialized
// descriptor here to reduce boilerplate and mistakes.
#define DEF_KEY(name, ...) \
  CallDescriptors::Key name##Descriptor::key() { return CallDescriptors::name; }
INTERFACE_DESCRIPTOR_LIST(DEF_KEY)
#undef DEF_KEY
}  // namespace internal
}  // namespace v8

#endif  // V8_INTERFACE_DESCRIPTORS_H_
