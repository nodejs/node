// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_INTERFACE_DESCRIPTORS_H_
#define V8_CODEGEN_INTERFACE_DESCRIPTORS_H_

#include <memory>

#include "src/base/logging.h"
#include "src/codegen/machine-type.h"
#include "src/codegen/register-arch.h"
#include "src/codegen/tnode.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"

namespace v8 {
namespace internal {

#define TORQUE_BUILTIN_LIST_TFC(V)                                            \
  BUILTIN_LIST_FROM_TORQUE(IGNORE_BUILTIN, IGNORE_BUILTIN, V, IGNORE_BUILTIN, \
                           IGNORE_BUILTIN, IGNORE_BUILTIN)

#define INTERFACE_DESCRIPTOR_LIST(V)     \
  V(Abort)                               \
  V(Allocate)                            \
  V(ApiCallback)                         \
  V(ApiGetter)                           \
  V(ArrayConstructor)                    \
  V(ArrayNArgumentsConstructor)          \
  V(ArrayNoArgumentConstructor)          \
  V(ArraySingleArgumentConstructor)      \
  V(AsyncFunctionStackParameter)         \
  V(BigIntToI32Pair)                     \
  V(BigIntToI64)                         \
  V(BinaryOp)                            \
  V(BinaryOp_Baseline)                   \
  V(BinaryOp_WithFeedback)               \
  V(CallForwardVarargs)                  \
  V(CallFunctionTemplate)                \
  V(CallTrampoline)                      \
  V(CallTrampoline_Baseline)             \
  V(CallTrampoline_Baseline_Compact)     \
  V(CallTrampoline_WithFeedback)         \
  V(CallVarargs)                         \
  V(CallWithArrayLike)                   \
  V(CallWithArrayLike_WithFeedback)      \
  V(CallWithSpread)                      \
  V(CallWithSpread_Baseline)             \
  V(CallWithSpread_WithFeedback)         \
  V(CEntry1ArgvOnStack)                  \
  V(CloneObjectBaseline)                 \
  V(CloneObjectWithVector)               \
  V(Compare)                             \
  V(Compare_Baseline)                    \
  V(Compare_WithFeedback)                \
  V(ConstructForwardVarargs)             \
  V(ConstructStub)                       \
  V(ConstructVarargs)                    \
  V(ConstructWithArrayLike)              \
  V(ConstructWithArrayLike_WithFeedback) \
  V(Construct_WithFeedback)              \
  V(Construct_Baseline)                  \
  V(ConstructWithSpread)                 \
  V(ConstructWithSpread_Baseline)        \
  V(ConstructWithSpread_WithFeedback)    \
  V(ContextOnly)                         \
  V(CppBuiltinAdaptor)                   \
  V(DynamicCheckMaps)                    \
  V(DynamicCheckMapsWithFeedbackVector)  \
  V(FastNewObject)                       \
  V(ForInPrepare)                        \
  V(GetIteratorStackParameter)           \
  V(GetProperty)                         \
  V(GrowArrayElements)                   \
  V(I32PairToBigInt)                     \
  V(I64ToBigInt)                         \
  V(InterpreterCEntry1)                  \
  V(InterpreterCEntry2)                  \
  V(InterpreterDispatch)                 \
  V(InterpreterPushArgsThenCall)         \
  V(InterpreterPushArgsThenConstruct)    \
  V(JSTrampoline)                        \
  V(BaselineOutOfLinePrologue)           \
  V(BaselineLeaveFrame)                  \
  V(Load)                                \
  V(LoadBaseline)                        \
  V(LoadGlobal)                          \
  V(LoadGlobalBaseline)                  \
  V(LoadGlobalNoFeedback)                \
  V(LoadGlobalWithVector)                \
  V(LoadNoFeedback)                      \
  V(LoadWithVector)                      \
  V(LoadWithReceiverAndVector)           \
  V(LoadWithReceiverBaseline)            \
  V(LookupBaseline)                      \
  V(NoContext)                           \
  V(ResumeGenerator)                     \
  V(SuspendGeneratorBaseline)            \
  V(ResumeGeneratorBaseline)             \
  V(RunMicrotasks)                       \
  V(RunMicrotasksEntry)                  \
  V(SingleParameterOnStack)              \
  V(Store)                               \
  V(StoreBaseline)                       \
  V(StoreGlobal)                         \
  V(StoreGlobalBaseline)                 \
  V(StoreGlobalWithVector)               \
  V(StoreTransition)                     \
  V(StoreWithVector)                     \
  V(StringAt)                            \
  V(StringAtAsString)                    \
  V(StringSubstring)                     \
  IF_TSAN(V, TSANRelaxedStore)           \
  IF_TSAN(V, TSANRelaxedLoad)            \
  V(TypeConversion)                      \
  V(TypeConversionNoContext)             \
  V(TypeConversion_Baseline)             \
  V(Typeof)                              \
  V(UnaryOp_Baseline)                    \
  V(UnaryOp_WithFeedback)                \
  V(Void)                                \
  V(WasmFloat32ToNumber)                 \
  V(WasmFloat64ToNumber)                 \
  V(WasmI32AtomicWait32)                 \
  V(WasmI64AtomicWait32)                 \
  V(WriteBarrier)                        \
  BUILTIN_LIST_TFS(V)                    \
  TORQUE_BUILTIN_LIST_TFC(V)

enum class StackArgumentOrder {
  kDefault,  // Arguments in the stack are pushed in the default/stub order (the
             // first argument is pushed first).
  kJS,  // Arguments in the stack are pushed in the same order as the one used
        // by JS-to-JS function calls. This should be used if calling a
        // JSFunction or if the builtin is expected to be called directly from a
        // JSFunction. This order is reversed compared to kDefault.
};

class V8_EXPORT_PRIVATE CallInterfaceDescriptorData {
 public:
  enum Flag {
    kNoFlags = 0u,
    kNoContext = 1u << 0,
    // This indicates that the code uses a special frame that does not scan the
    // stack arguments, e.g. EntryFrame. And this allows the code to use
    // untagged stack arguments.
    kNoStackScan = 1u << 1,
    // In addition to the specified parameters, additional arguments can be
    // passed on the stack.
    // This does not indicate if arguments adaption is used or not.
    kAllowVarArgs = 1u << 2,
    // Callee save allocatable_registers.
    kCalleeSaveRegisters = 1u << 3,
  };
  using Flags = base::Flags<Flag>;

  static constexpr int kUninitializedCount = -1;

  CallInterfaceDescriptorData() = default;

  CallInterfaceDescriptorData(const CallInterfaceDescriptorData&) = delete;
  CallInterfaceDescriptorData& operator=(const CallInterfaceDescriptorData&) =
      delete;

  // The passed registers are owned by the caller, and their lifetime is
  // expected to exceed that of this data. In practice, they are expected to
  // be in a static local.
  void InitializeRegisters(Flags flags, int return_count, int parameter_count,
                           StackArgumentOrder stack_order,
                           int register_parameter_count,
                           const Register* registers);

  // if machine_types is null, then an array of size
  // (return_count + parameter_count) will be created with
  // MachineType::AnyTagged() for each member.
  //
  // if machine_types is not null, then it should be of the size
  // (return_count + parameter_count). Those members of the parameter array will
  // be initialized from {machine_types}, and the rest initialized to
  // MachineType::AnyTagged().
  void InitializeTypes(const MachineType* machine_types,
                       int machine_types_length);

  void Reset();

  bool IsInitialized() const {
    return IsInitializedRegisters() && IsInitializedTypes();
  }

  Flags flags() const { return flags_; }
  int return_count() const { return return_count_; }
  int param_count() const { return param_count_; }
  int register_param_count() const { return register_param_count_; }
  Register register_param(int index) const { return register_params_[index]; }
  MachineType return_type(int index) const {
    DCHECK_LT(index, return_count_);
    return machine_types_[index];
  }
  MachineType param_type(int index) const {
    DCHECK_LT(index, param_count_);
    return machine_types_[return_count_ + index];
  }
  StackArgumentOrder stack_order() const { return stack_order_; }

  void RestrictAllocatableRegisters(const Register* registers, size_t num) {
    DCHECK_EQ(allocatable_registers_, 0);
    for (size_t i = 0; i < num; ++i) {
      allocatable_registers_ |= registers[i].bit();
    }
    DCHECK_GT(NumRegs(allocatable_registers_), 0);
  }

  RegList allocatable_registers() const { return allocatable_registers_; }

 private:
  bool IsInitializedRegisters() const {
    const bool initialized =
        return_count_ != kUninitializedCount &&
        param_count_ != kUninitializedCount &&
        (register_param_count_ == 0 || register_params_ != nullptr);
    // Register initialization happens before type initialization.
    return initialized;
  }
  bool IsInitializedTypes() const {
    const bool initialized = machine_types_ != nullptr;
    // Register initialization happens before type initialization.
    return initialized;
  }

#ifdef DEBUG
  bool AllStackParametersAreTagged() const;
#endif  // DEBUG

  int register_param_count_ = kUninitializedCount;
  int return_count_ = kUninitializedCount;
  int param_count_ = kUninitializedCount;
  Flags flags_ = kNoFlags;
  StackArgumentOrder stack_order_ = StackArgumentOrder::kDefault;

  // Specifying the set of registers that could be used by the register
  // allocator. Currently, it's only used by RecordWrite code stub.
  RegList allocatable_registers_ = 0;

  // |registers_params_| defines registers that are used for parameter passing.
  // |machine_types_| defines machine types for resulting values and incomping
  // parameters.
  // The register params array is owned by the caller, and it's expected that it
  // is a static local stored in the caller function. The machine types are
  // allocated dynamically by the InterfaceDescriptor and freed on destruction.
  const Register* register_params_ = nullptr;
  MachineType* machine_types_ = nullptr;
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

#if defined(V8_TARGET_ARCH_IA32)
// To support all possible cases, we must limit the number of register args for
// TFS builtins on ia32 to 3. Out of the 6 allocatable registers, esi is taken
// as the context register and ebx is the root register. One register must
// remain available to store the jump/call target. Thus 3 registers remain for
// arguments. The reason this applies to TFS builtins specifically is because
// this becomes relevant for builtins used as targets of Torque function
// pointers (which must have a register available to store the target).
// TODO(jgruber): Ideally we should just decrement kMaxBuiltinRegisterParams but
// that comes with its own set of complications. It's possible, but requires
// refactoring the calling convention of other existing stubs.
constexpr int kMaxBuiltinRegisterParams = 4;
constexpr int kMaxTFSBuiltinRegisterParams = 3;
#else
constexpr int kMaxBuiltinRegisterParams = 5;
constexpr int kMaxTFSBuiltinRegisterParams = kMaxBuiltinRegisterParams;
#endif
STATIC_ASSERT(kMaxTFSBuiltinRegisterParams <= kMaxBuiltinRegisterParams);
constexpr int kJSBuiltinRegisterParams = 4;

// Polymorphic base class for call interface descriptors, which defines getters
// for the various descriptor properties via a runtime-loaded
// CallInterfaceDescriptorData field.
class V8_EXPORT_PRIVATE CallInterfaceDescriptor {
 public:
  using Flags = CallInterfaceDescriptorData::Flags;

  CallInterfaceDescriptor() : data_(nullptr) {}
  ~CallInterfaceDescriptor() = default;

  explicit CallInterfaceDescriptor(CallDescriptors::Key key)
      : data_(CallDescriptors::call_descriptor_data(key)) {}

  Flags flags() const { return data()->flags(); }

  bool HasContextParameter() const {
    return (flags() & CallInterfaceDescriptorData::kNoContext) == 0;
  }

  bool AllowVarArgs() const {
    return flags() & CallInterfaceDescriptorData::kAllowVarArgs;
  }

  bool CalleeSaveRegisters() const {
    return flags() & CallInterfaceDescriptorData::kCalleeSaveRegisters;
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
    DCHECK_LT(index, data()->register_param_count());
    return data()->register_param(index);
  }

  MachineType GetParameterType(int index) const {
    DCHECK_LT(index, data()->param_count());
    return data()->param_type(index);
  }

  RegList allocatable_registers() const {
    return data()->allocatable_registers();
  }

  StackArgumentOrder GetStackArgumentOrder() const {
    return data()->stack_order();
  }

  static constexpr inline Register ContextRegister() {
    return kContextRegister;
  }

  const char* DebugName() const;

  bool operator==(const CallInterfaceDescriptor& other) const {
    return data() == other.data();
  }

 protected:
  const CallInterfaceDescriptorData* data() const { return data_; }

  // Helper for defining the default register set.
  //
  // Use auto for the return type to allow different architectures to have
  // differently sized default register arrays.
  static constexpr inline auto DefaultRegisterArray();
  static constexpr inline std::array<Register, kJSBuiltinRegisterParams>
  DefaultJSRegisterArray();

  // Checks if float parameters are not assigned invalid registers.
  bool CheckFloatingPointParameters(CallInterfaceDescriptorData* data) {
    for (int i = 0; i < data->register_param_count(); i++) {
      if (IsFloatingPoint(data->param_type(i).representation())) {
        if (!IsValidFloatParameterRegister(data->register_param(i))) {
          return false;
        }
      }
    }
    return true;
  }

  bool IsValidFloatParameterRegister(Register reg);

 private:
  const CallInterfaceDescriptorData* data_;
};

// CRTP base class for call interface descriptors, which defines static getters
// for the various descriptor properties based on static values defined in the
// subclass.
template <typename DerivedDescriptor>
class StaticCallInterfaceDescriptor : public CallInterfaceDescriptor {
 public:
  // ===========================================================================
  // The following are the descriptor's CRTP configuration points, overwritable
  // by DerivedDescriptor.
  static constexpr int kReturnCount =
      CallInterfaceDescriptorData::kUninitializedCount;
  static constexpr int kParameterCount =
      CallInterfaceDescriptorData::kUninitializedCount;
  static constexpr bool kNoContext = false;
  static constexpr bool kAllowVarArgs = false;
  static constexpr bool kNoStackScan = false;
  static constexpr auto kStackArgumentOrder = StackArgumentOrder::kDefault;

  // The set of registers available to the parameters, as a
  // std::array<Register,N>. Can be larger or smaller than kParameterCount; if
  // larger then any remaining registers are ignored; if smaller, any parameters
  // after registers().size() will be stack registers.
  //
  // Defaults to CallInterfaceDescriptor::DefaultRegisterArray().
  static constexpr inline auto registers();

  // An additional limit on the number of register parameters allowed. This is
  // here so that it can be overwritten to kMaxTFSBuiltinRegisterParams for TFS
  // builtins, see comment on kMaxTFSBuiltinRegisterParams above.
  static constexpr int kMaxRegisterParams = kMaxBuiltinRegisterParams;

  // If set to true, the descriptor will restrict the set of allocatable
  // registers to the set returned by registers(). Then, it is expected that
  // the first kParameterCount registers() are the parameters of the builtin.
  static constexpr bool kRestrictAllocatableRegisters = false;

  // If set to true, builtins will callee save the set returned by registers().
  static constexpr bool kCalleeSaveRegisters = false;

  // End of customization points.
  // ===========================================================================

  static constexpr inline Flags flags() {
    return Flags((DerivedDescriptor::kNoContext
                      ? CallInterfaceDescriptorData::kNoContext
                      : 0) |
                 (DerivedDescriptor::kAllowVarArgs
                      ? CallInterfaceDescriptorData::kAllowVarArgs
                      : 0) |
                 (DerivedDescriptor::kNoStackScan
                      ? CallInterfaceDescriptorData::kNoStackScan
                      : 0) |
                 (DerivedDescriptor::kCalleeSaveRegisters
                      ? CallInterfaceDescriptorData::kCalleeSaveRegisters
                      : 0));
  }
  static constexpr inline bool AllowVarArgs() {
    return DerivedDescriptor::kAllowVarArgs;
  }
  static constexpr inline bool HasContextParameter() {
    return !DerivedDescriptor::kNoContext;
  }

  static constexpr inline int GetReturnCount();
  static constexpr inline int GetParameterCount();
  static constexpr inline int GetRegisterParameterCount();
  static constexpr inline int GetStackParameterCount();
  static constexpr inline Register* GetRegisterData();
  static constexpr inline Register GetRegisterParameter(int i);

  explicit StaticCallInterfaceDescriptor(CallDescriptors::Key key)
      : CallInterfaceDescriptor(key) {}

#if DEBUG
  // Overwritten in DerivedDescriptor.
  static void Verify(CallInterfaceDescriptorData* data);
  // Verify that the CallInterfaceDescriptorData contains the default
  // argument registers for {argc} arguments.
  static inline void VerifyArgumentRegisterCount(
      CallInterfaceDescriptorData* data, int nof_expected_args);
#endif

 private:
  // {CallDescriptors} is allowed to call the private {Initialize} method.
  friend class CallDescriptors;

  inline void Initialize(CallInterfaceDescriptorData* data);

  // Set up the types of the descriptor. This is a static function, so that it
  // is overwritable by subclasses. By default, all parameters have
  // MachineType::AnyTagged() type.
  static void InitializeTypes(CallInterfaceDescriptorData* data) {
    data->InitializeTypes(nullptr, 0);
  }
};

template <typename Descriptor>
class StaticJSCallInterfaceDescriptor
    : public StaticCallInterfaceDescriptor<Descriptor> {
 public:
  static constexpr auto kStackArgumentOrder = StackArgumentOrder::kJS;
  static constexpr inline auto registers();

  using StaticCallInterfaceDescriptor<
      Descriptor>::StaticCallInterfaceDescriptor;
};

template <Builtin kBuiltin>
struct CallInterfaceDescriptorFor;

// Stub class replacing std::array<Register, 0>, as a workaround for MSVC's
// https://github.com/microsoft/STL/issues/942
struct EmptyRegisterArray {
  Register* data() { return nullptr; }
  size_t size() const { return 0; }
  Register operator[](size_t i) const { UNREACHABLE(); }
};

// Helper method for defining an array of unique registers for the various
// Descriptor::registers() methods.
template <typename... Registers>
constexpr std::array<Register, 1 + sizeof...(Registers)> RegisterArray(
    Register first_reg, Registers... regs) {
  DCHECK(!AreAliased(first_reg, regs...));
  return {first_reg, regs...};
}
constexpr EmptyRegisterArray RegisterArray() { return {}; }

#define DECLARE_DESCRIPTOR_WITH_BASE(name, base)                  \
 public:                                                          \
  /* StaticCallInterfaceDescriptor can call Initialize methods */ \
  friend class StaticCallInterfaceDescriptor<name>;               \
  explicit name() : base(key()) {}                                \
  static inline CallDescriptors::Key key();

#define DECLARE_DEFAULT_DESCRIPTOR(name)                                  \
  DECLARE_DESCRIPTOR_WITH_BASE(name, StaticCallInterfaceDescriptor)       \
  static constexpr int kMaxRegisterParams = kMaxTFSBuiltinRegisterParams; \
                                                                          \
 protected:                                                               \
  explicit name(CallDescriptors::Key key)                                 \
      : StaticCallInterfaceDescriptor(key) {}                             \
                                                                          \
 public:

#define DECLARE_JS_COMPATIBLE_DESCRIPTOR(name)                        \
  DECLARE_DESCRIPTOR_WITH_BASE(name, StaticJSCallInterfaceDescriptor) \
 protected:                                                           \
  explicit name(CallDescriptors::Key key)                             \
      : StaticJSCallInterfaceDescriptor(key) {}                       \
                                                                      \
 public:

#define DEFINE_RESULT_AND_PARAMETERS(return_count, ...)   \
  static constexpr int kReturnCount = return_count;       \
  enum ParameterIndices {                                 \
    __dummy = -1, /* to be able to pass zero arguments */ \
    ##__VA_ARGS__,                                        \
                                                          \
    kParameterCount,                                      \
    kContext = kParameterCount /* implicit parameter */   \
  };

// This is valid only for builtins that use EntryFrame, which does not scan
// stack arguments on GC.
#define DEFINE_PARAMETERS_ENTRY(...)                        \
  static constexpr bool kNoContext = true;                  \
  static constexpr bool kNoStackScan = true;                \
  static constexpr StackArgumentOrder kStackArgumentOrder = \
      StackArgumentOrder::kDefault;                         \
  static constexpr int kReturnCount = 1;                    \
  enum ParameterIndices {                                   \
    __dummy = -1, /* to be able to pass zero arguments */   \
    ##__VA_ARGS__,                                          \
                                                            \
    kParameterCount                                         \
  };

#define DEFINE_PARAMETERS(...) DEFINE_RESULT_AND_PARAMETERS(1, ##__VA_ARGS__)

#define DEFINE_PARAMETERS_NO_CONTEXT(...) \
  DEFINE_PARAMETERS(__VA_ARGS__)          \
  static constexpr bool kNoContext = true;

#define DEFINE_PARAMETERS_VARARGS(...)                      \
  DEFINE_PARAMETERS(__VA_ARGS__)                            \
  static constexpr bool kAllowVarArgs = true;               \
  static constexpr StackArgumentOrder kStackArgumentOrder = \
      StackArgumentOrder::kJS;

#define DEFINE_PARAMETERS_NO_CONTEXT_VARARGS(...)           \
  DEFINE_PARAMETERS_NO_CONTEXT(__VA_ARGS__)                 \
  static constexpr bool kAllowVarArgs = true;               \
  static constexpr StackArgumentOrder kStackArgumentOrder = \
      StackArgumentOrder::kJS;

#define DEFINE_RESULT_AND_PARAMETERS_NO_CONTEXT(return_count, ...) \
  DEFINE_RESULT_AND_PARAMETERS(return_count, ##__VA_ARGS__)        \
  static constexpr bool kNoContext = true;

#define DEFINE_RESULT_AND_PARAMETER_TYPES(...)                                \
  static void InitializeTypes(CallInterfaceDescriptorData* data) {            \
    MachineType machine_types[] = {__VA_ARGS__};                              \
    static_assert(                                                            \
        kReturnCount + kParameterCount == arraysize(machine_types),           \
        "Parameter names definition is not consistent with parameter types"); \
    data->InitializeTypes(machine_types, arraysize(machine_types));           \
  }

#define DEFINE_PARAMETER_TYPES(...)                                        \
  DEFINE_RESULT_AND_PARAMETER_TYPES(MachineType::AnyTagged() /* result */, \
                                    ##__VA_ARGS__)

// When the extra arguments described here are located in the stack, they are
// just above the return address in the frame (first arguments).
#define DEFINE_JS_PARAMETERS(...)                           \
  static constexpr bool kAllowVarArgs = true;               \
  static constexpr int kReturnCount = 1;                    \
  static constexpr StackArgumentOrder kStackArgumentOrder = \
      StackArgumentOrder::kJS;                              \
  enum ParameterIndices {                                   \
    kTarget,                                                \
    kNewTarget,                                             \
    kActualArgumentsCount,                                  \
    ##__VA_ARGS__,                                          \
    kParameterCount,                                        \
    kContext = kParameterCount /* implicit parameter */     \
  };

#define DEFINE_JS_PARAMETERS_NO_CONTEXT(...)                \
  static constexpr bool kAllowVarArgs = true;               \
  static constexpr bool kNoContext = true;                  \
  static constexpr int kReturnCount = 1;                    \
  static constexpr StackArgumentOrder kStackArgumentOrder = \
      StackArgumentOrder::kJS;                              \
  enum ParameterIndices {                                   \
    kTarget,                                                \
    kNewTarget,                                             \
    kActualArgumentsCount,                                  \
    ##__VA_ARGS__,                                          \
    kParameterCount,                                        \
  };

#define DEFINE_JS_PARAMETER_TYPES(...)                                         \
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(), /* kTarget */               \
                         MachineType::AnyTagged(), /* kNewTarget */            \
                         MachineType::Int32(),     /* kActualArgumentsCount */ \
                         ##__VA_ARGS__)

#define DECLARE_DESCRIPTOR(name)                                    \
  DECLARE_DESCRIPTOR_WITH_BASE(name, StaticCallInterfaceDescriptor) \
 protected:                                                         \
  explicit name(CallDescriptors::Key key)                           \
      : StaticCallInterfaceDescriptor(key) {}                       \
                                                                    \
 public:

class V8_EXPORT_PRIVATE VoidDescriptor
    : public StaticCallInterfaceDescriptor<VoidDescriptor> {
 public:
  DEFINE_PARAMETERS()
  DEFINE_PARAMETER_TYPES()
  DECLARE_DESCRIPTOR(VoidDescriptor)

  static constexpr auto registers();
};

// Dummy descriptor used to mark builtins that don't yet have their proper
// descriptor associated.
using DummyDescriptor = VoidDescriptor;

// Dummy descriptor that marks builtins with C calling convention.
using CCallDescriptor = VoidDescriptor;

// Marks deoptimization entry builtins. Precise calling conventions currently
// differ based on the platform.
// TODO(jgruber): Once this is unified, we could create a better description
// here.
using DeoptimizationEntryDescriptor = VoidDescriptor;

class AllocateDescriptor
    : public StaticCallInterfaceDescriptor<AllocateDescriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kRequestedSize)
  DEFINE_RESULT_AND_PARAMETER_TYPES(MachineType::TaggedPointer(),  // result 1
                                    MachineType::IntPtr())  // kRequestedSize
  DECLARE_DESCRIPTOR(AllocateDescriptor)

  static constexpr auto registers();
};

// This descriptor defines the JavaScript calling convention that can be used
// by stubs: target, new.target, argc (not including the receiver) and context
// are passed in registers while receiver and the rest of the JS arguments are
// passed on the stack.
class JSTrampolineDescriptor
    : public StaticJSCallInterfaceDescriptor<JSTrampolineDescriptor> {
 public:
  DEFINE_JS_PARAMETERS()
  DEFINE_JS_PARAMETER_TYPES()

  DECLARE_JS_COMPATIBLE_DESCRIPTOR(JSTrampolineDescriptor)
};

class ContextOnlyDescriptor
    : public StaticCallInterfaceDescriptor<ContextOnlyDescriptor> {
 public:
  DEFINE_PARAMETERS()
  DEFINE_PARAMETER_TYPES()
  DECLARE_DESCRIPTOR(ContextOnlyDescriptor)

  static constexpr auto registers();
};

class NoContextDescriptor
    : public StaticCallInterfaceDescriptor<NoContextDescriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT()
  DEFINE_PARAMETER_TYPES()
  DECLARE_DESCRIPTOR(NoContextDescriptor)

  static constexpr auto registers();
};

// LoadDescriptor is used by all stubs that implement Load/KeyedLoad ICs.
class LoadDescriptor : public StaticCallInterfaceDescriptor<LoadDescriptor> {
 public:
  DEFINE_PARAMETERS(kReceiver, kName, kSlot)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),     // kReceiver
                         MachineType::AnyTagged(),     // kName
                         MachineType::TaggedSigned())  // kSlot
  DECLARE_DESCRIPTOR(LoadDescriptor)

  static constexpr inline Register ReceiverRegister();
  static constexpr inline Register NameRegister();
  static constexpr inline Register SlotRegister();

  static constexpr auto registers();
};

// LoadBaselineDescriptor is a load descriptor that does not take a context as
// input.
class LoadBaselineDescriptor
    : public StaticCallInterfaceDescriptor<LoadBaselineDescriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kReceiver, kName, kSlot)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),     // kReceiver
                         MachineType::AnyTagged(),     // kName
                         MachineType::TaggedSigned())  // kSlot
  DECLARE_DESCRIPTOR(LoadBaselineDescriptor)

  static constexpr auto registers();
};

class LoadGlobalNoFeedbackDescriptor
    : public StaticCallInterfaceDescriptor<LoadGlobalNoFeedbackDescriptor> {
 public:
  DEFINE_PARAMETERS(kName, kICKind)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),     // kName
                         MachineType::TaggedSigned())  // kICKind
  DECLARE_DESCRIPTOR(LoadGlobalNoFeedbackDescriptor)

  static constexpr inline Register ICKindRegister();

  static constexpr auto registers();
};

class LoadNoFeedbackDescriptor
    : public StaticCallInterfaceDescriptor<LoadNoFeedbackDescriptor> {
 public:
  DEFINE_PARAMETERS(kReceiver, kName, kICKind)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),     // kReceiver
                         MachineType::AnyTagged(),     // kName
                         MachineType::TaggedSigned())  // kICKind
  DECLARE_DESCRIPTOR(LoadNoFeedbackDescriptor)

  static constexpr inline Register ICKindRegister();

  static constexpr auto registers();
};

class LoadGlobalDescriptor
    : public StaticCallInterfaceDescriptor<LoadGlobalDescriptor> {
 public:
  DEFINE_PARAMETERS(kName, kSlot)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),     // kName
                         MachineType::TaggedSigned())  // kSlot
  DECLARE_DESCRIPTOR(LoadGlobalDescriptor)

  static constexpr auto registers();
};

class LoadGlobalBaselineDescriptor
    : public StaticCallInterfaceDescriptor<LoadGlobalBaselineDescriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kName, kSlot)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),     // kName
                         MachineType::TaggedSigned())  // kSlot
  DECLARE_DESCRIPTOR(LoadGlobalBaselineDescriptor)

  static constexpr auto registers();
};

class LookupBaselineDescriptor
    : public StaticCallInterfaceDescriptor<LookupBaselineDescriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kName, kDepth, kSlot)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kName
                         MachineType::AnyTagged(),  // kDepth
                         MachineType::AnyTagged())  // kSlot
  DECLARE_DESCRIPTOR(LookupBaselineDescriptor)
};

class StoreDescriptor : public StaticCallInterfaceDescriptor<StoreDescriptor> {
 public:
  DEFINE_PARAMETERS(kReceiver, kName, kValue, kSlot)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),     // kReceiver
                         MachineType::AnyTagged(),     // kName
                         MachineType::AnyTagged(),     // kValue
                         MachineType::TaggedSigned())  // kSlot
  DECLARE_DESCRIPTOR(StoreDescriptor)

  static constexpr inline Register ReceiverRegister();
  static constexpr inline Register NameRegister();
  static constexpr inline Register ValueRegister();
  static constexpr inline Register SlotRegister();

  static constexpr auto registers();
};

class StoreBaselineDescriptor
    : public StaticCallInterfaceDescriptor<StoreBaselineDescriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kReceiver, kName, kValue, kSlot)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),     // kReceiver
                         MachineType::AnyTagged(),     // kName
                         MachineType::AnyTagged(),     // kValue
                         MachineType::TaggedSigned())  // kSlot
  DECLARE_DESCRIPTOR(StoreBaselineDescriptor)

  static constexpr auto registers();
};

class StoreTransitionDescriptor
    : public StaticCallInterfaceDescriptor<StoreTransitionDescriptor> {
 public:
  DEFINE_PARAMETERS(kReceiver, kName, kMap, kValue, kSlot, kVector)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),     // kReceiver
                         MachineType::AnyTagged(),     // kName
                         MachineType::AnyTagged(),     // kMap
                         MachineType::AnyTagged(),     // kValue
                         MachineType::TaggedSigned(),  // kSlot
                         MachineType::AnyTagged())     // kVector
  DECLARE_DESCRIPTOR(StoreTransitionDescriptor)

  static constexpr inline Register MapRegister();

  static constexpr auto registers();
};

class StoreWithVectorDescriptor
    : public StaticCallInterfaceDescriptor<StoreWithVectorDescriptor> {
 public:
  DEFINE_PARAMETERS(kReceiver, kName, kValue, kSlot, kVector)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),     // kReceiver
                         MachineType::AnyTagged(),     // kName
                         MachineType::AnyTagged(),     // kValue
                         MachineType::TaggedSigned(),  // kSlot
                         MachineType::AnyTagged())     // kVector
  DECLARE_DESCRIPTOR(StoreWithVectorDescriptor)

  static constexpr inline Register VectorRegister();

  static constexpr auto registers();
};

class StoreGlobalDescriptor
    : public StaticCallInterfaceDescriptor<StoreGlobalDescriptor> {
 public:
  DEFINE_PARAMETERS(kName, kValue, kSlot)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),     // kName
                         MachineType::AnyTagged(),     // kValue
                         MachineType::TaggedSigned())  // kSlot
  DECLARE_DESCRIPTOR(StoreGlobalDescriptor)

  static constexpr auto registers();
};

class StoreGlobalBaselineDescriptor
    : public StaticCallInterfaceDescriptor<StoreGlobalBaselineDescriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kName, kValue, kSlot)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),     // kName
                         MachineType::AnyTagged(),     // kValue
                         MachineType::TaggedSigned())  // kSlot
  DECLARE_DESCRIPTOR(StoreGlobalBaselineDescriptor)

  static constexpr auto registers();
};

class StoreGlobalWithVectorDescriptor
    : public StaticCallInterfaceDescriptor<StoreGlobalWithVectorDescriptor> {
 public:
  DEFINE_PARAMETERS(kName, kValue, kSlot, kVector)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),     // kName
                         MachineType::AnyTagged(),     // kValue
                         MachineType::TaggedSigned(),  // kSlot
                         MachineType::AnyTagged())     // kVector
  DECLARE_DESCRIPTOR(StoreGlobalWithVectorDescriptor)

  static constexpr auto registers();
};

class LoadWithVectorDescriptor
    : public StaticCallInterfaceDescriptor<LoadWithVectorDescriptor> {
 public:
  // TODO(v8:9497): Revert the Machine type for kSlot to the
  // TaggedSigned once Torque can emit better call descriptors
  DEFINE_PARAMETERS(kReceiver, kName, kSlot, kVector)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kReceiver
                         MachineType::AnyTagged(),  // kName
                         MachineType::AnyTagged(),  // kSlot
                         MachineType::AnyTagged())  // kVector
  DECLARE_DESCRIPTOR(LoadWithVectorDescriptor)

  static constexpr inline Register VectorRegister();

  static constexpr auto registers();
};

// Like LoadWithVectorDescriptor, except we pass the receiver (the object which
// should be used as the receiver for accessor function calls) and the lookup
// start object separately.
class LoadWithReceiverAndVectorDescriptor
    : public StaticCallInterfaceDescriptor<
          LoadWithReceiverAndVectorDescriptor> {
 public:
  // TODO(v8:9497): Revert the Machine type for kSlot to the
  // TaggedSigned once Torque can emit better call descriptors
  DEFINE_PARAMETERS(kReceiver, kLookupStartObject, kName, kSlot, kVector)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kReceiver
                         MachineType::AnyTagged(),  // kLookupStartObject
                         MachineType::AnyTagged(),  // kName
                         MachineType::AnyTagged(),  // kSlot
                         MachineType::AnyTagged())  // kVector
  DECLARE_DESCRIPTOR(LoadWithReceiverAndVectorDescriptor)

  static constexpr inline Register LookupStartObjectRegister();

  static constexpr auto registers();
};

class LoadWithReceiverBaselineDescriptor
    : public StaticCallInterfaceDescriptor<LoadWithReceiverBaselineDescriptor> {
 public:
  // TODO(v8:9497): Revert the Machine type for kSlot to the
  // TaggedSigned once Torque can emit better call descriptors
  DEFINE_PARAMETERS_NO_CONTEXT(kReceiver, kLookupStartObject, kName, kSlot)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kReceiver
                         MachineType::AnyTagged(),  // kLookupStartObject
                         MachineType::AnyTagged(),  // kName
                         MachineType::AnyTagged())  // kSlot
  DECLARE_DESCRIPTOR(LoadWithReceiverBaselineDescriptor)

  static constexpr auto registers();
};

class LoadGlobalWithVectorDescriptor
    : public StaticCallInterfaceDescriptor<LoadGlobalWithVectorDescriptor> {
 public:
  DEFINE_PARAMETERS(kName, kSlot, kVector)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),     // kName
                         MachineType::TaggedSigned(),  // kSlot
                         MachineType::AnyTagged())     // kVector
  DECLARE_DESCRIPTOR(LoadGlobalWithVectorDescriptor)

  static constexpr inline Register VectorRegister();

  static constexpr auto registers();
};

class DynamicCheckMapsDescriptor final
    : public StaticCallInterfaceDescriptor<DynamicCheckMapsDescriptor> {
 public:
  DEFINE_PARAMETERS(kMap, kSlot, kHandler)
  DEFINE_RESULT_AND_PARAMETER_TYPES(MachineType::Int32(),          // return val
                                    MachineType::TaggedPointer(),  // kMap
                                    MachineType::IntPtr(),         // kSlot
                                    MachineType::TaggedSigned())   // kHandler

  DECLARE_DESCRIPTOR(DynamicCheckMapsDescriptor)

  static constexpr auto registers();
  static constexpr bool kRestrictAllocatableRegisters = true;
};

class DynamicCheckMapsWithFeedbackVectorDescriptor final
    : public StaticCallInterfaceDescriptor<
          DynamicCheckMapsWithFeedbackVectorDescriptor> {
 public:
  DEFINE_PARAMETERS(kMap, kFeedbackVector, kSlot, kHandler)
  DEFINE_RESULT_AND_PARAMETER_TYPES(
      MachineType::Int32(),          // return val
      MachineType::TaggedPointer(),  // kMap
      MachineType::TaggedPointer(),  // kFeedbackVector
      MachineType::IntPtr(),         // kSlot
      MachineType::TaggedSigned())   // kHandler

  DECLARE_DESCRIPTOR(DynamicCheckMapsWithFeedbackVectorDescriptor)

  static constexpr auto registers();
  static constexpr bool kRestrictAllocatableRegisters = true;
};

class FastNewObjectDescriptor
    : public StaticCallInterfaceDescriptor<FastNewObjectDescriptor> {
 public:
  DEFINE_PARAMETERS(kTarget, kNewTarget)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kTarget
                         MachineType::AnyTagged())  // kNewTarget
  DECLARE_DESCRIPTOR(FastNewObjectDescriptor)

  static constexpr inline Register TargetRegister();
  static constexpr inline Register NewTargetRegister();

  static constexpr auto registers();
};

class WriteBarrierDescriptor final
    : public StaticCallInterfaceDescriptor<WriteBarrierDescriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kObject, kSlotAddress)
  DEFINE_PARAMETER_TYPES(MachineType::TaggedPointer(),  // kObject
                         MachineType::Pointer())        // kSlotAddress

  DECLARE_DESCRIPTOR(WriteBarrierDescriptor)
  static constexpr auto registers();
  static constexpr bool kRestrictAllocatableRegisters = true;
  static constexpr bool kCalleeSaveRegisters = true;
  static constexpr inline Register ObjectRegister();
  static constexpr inline Register SlotAddressRegister();
  // A temporary register used in helpers.
  static constexpr inline Register ValueRegister();
  static constexpr inline RegList ComputeSavedRegisters(
      Register object, Register slot_address = no_reg);
#if DEBUG
  static void Verify(CallInterfaceDescriptorData* data);
#endif
};

#ifdef V8_IS_TSAN
class TSANRelaxedStoreDescriptor final
    : public StaticCallInterfaceDescriptor<TSANRelaxedStoreDescriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kAddress, kValue)
  DEFINE_PARAMETER_TYPES(MachineType::Pointer(),    // kAddress
                         MachineType::AnyTagged())  // kValue

  DECLARE_DESCRIPTOR(TSANRelaxedStoreDescriptor)

  static constexpr auto registers();
  static constexpr bool kRestrictAllocatableRegisters = true;
};

class TSANRelaxedLoadDescriptor final
    : public StaticCallInterfaceDescriptor<TSANRelaxedLoadDescriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kAddress)
  DEFINE_PARAMETER_TYPES(MachineType::Pointer())  // kAddress

  DECLARE_DESCRIPTOR(TSANRelaxedLoadDescriptor)

  static constexpr auto registers();
  static constexpr bool kRestrictAllocatableRegisters = true;
};

#endif  // V8_IS_TSAN

class TypeConversionDescriptor final
    : public StaticCallInterfaceDescriptor<TypeConversionDescriptor> {
 public:
  DEFINE_PARAMETERS(kArgument)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged())
  DECLARE_DESCRIPTOR(TypeConversionDescriptor)

  static constexpr inline Register ArgumentRegister();

  static constexpr auto registers();
};

class TypeConversionNoContextDescriptor final
    : public StaticCallInterfaceDescriptor<TypeConversionNoContextDescriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kArgument)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged())
  DECLARE_DESCRIPTOR(TypeConversionNoContextDescriptor)

  static constexpr auto registers();
};

class TypeConversion_BaselineDescriptor final
    : public StaticCallInterfaceDescriptor<TypeConversion_BaselineDescriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kArgument, kSlot)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(), MachineType::UintPtr())
  DECLARE_DESCRIPTOR(TypeConversion_BaselineDescriptor)
};

class SingleParameterOnStackDescriptor final
    : public StaticCallInterfaceDescriptor<SingleParameterOnStackDescriptor> {
 public:
  DEFINE_PARAMETERS(kArgument)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged())
  DECLARE_DESCRIPTOR(SingleParameterOnStackDescriptor)

  static constexpr auto registers();
};

class AsyncFunctionStackParameterDescriptor final
    : public StaticCallInterfaceDescriptor<
          AsyncFunctionStackParameterDescriptor> {
 public:
  DEFINE_PARAMETERS(kPromise, kResult)
  DEFINE_PARAMETER_TYPES(MachineType::TaggedPointer(), MachineType::AnyTagged())
  DECLARE_DESCRIPTOR(AsyncFunctionStackParameterDescriptor)

  static constexpr auto registers();
};

class GetIteratorStackParameterDescriptor final
    : public StaticCallInterfaceDescriptor<
          GetIteratorStackParameterDescriptor> {
 public:
  DEFINE_PARAMETERS(kReceiver, kCallSlot, kFeedback, kResult)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(), MachineType::AnyTagged(),
                         MachineType::AnyTagged(), MachineType::AnyTagged())
  DECLARE_DESCRIPTOR(GetIteratorStackParameterDescriptor)

  static constexpr auto registers();
};

class GetPropertyDescriptor final
    : public StaticCallInterfaceDescriptor<GetPropertyDescriptor> {
 public:
  DEFINE_PARAMETERS(kObject, kKey)
  DECLARE_DEFAULT_DESCRIPTOR(GetPropertyDescriptor)
};

class TypeofDescriptor
    : public StaticCallInterfaceDescriptor<TypeofDescriptor> {
 public:
  DEFINE_PARAMETERS(kObject)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged())
  DECLARE_DESCRIPTOR(TypeofDescriptor)

  static constexpr inline auto registers();
};

class CallTrampolineDescriptor
    : public StaticCallInterfaceDescriptor<CallTrampolineDescriptor> {
 public:
  DEFINE_PARAMETERS_VARARGS(kFunction, kActualArgumentsCount)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kFunction
                         MachineType::Int32())      // kActualArgumentsCount
  DECLARE_DESCRIPTOR(CallTrampolineDescriptor)

  static constexpr inline auto registers();
};

class CallVarargsDescriptor
    : public StaticCallInterfaceDescriptor<CallVarargsDescriptor> {
 public:
  DEFINE_PARAMETERS_VARARGS(kTarget, kActualArgumentsCount, kArgumentsLength,
                            kArgumentsList)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kTarget
                         MachineType::Int32(),      // kActualArgumentsCount
                         MachineType::Int32(),      // kArgumentsLength
                         MachineType::AnyTagged())  // kArgumentsList
  DECLARE_DESCRIPTOR(CallVarargsDescriptor)

  static constexpr inline auto registers();
};

class CallForwardVarargsDescriptor
    : public StaticCallInterfaceDescriptor<CallForwardVarargsDescriptor> {
 public:
  DEFINE_PARAMETERS_VARARGS(kTarget, kActualArgumentsCount, kStartIndex)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kTarget
                         MachineType::Int32(),      // kActualArgumentsCount
                         MachineType::Int32())      // kStartIndex
  DECLARE_DESCRIPTOR(CallForwardVarargsDescriptor)

  static constexpr inline auto registers();
};

class CallFunctionTemplateDescriptor
    : public StaticCallInterfaceDescriptor<CallFunctionTemplateDescriptor> {
 public:
  DEFINE_PARAMETERS_VARARGS(kFunctionTemplateInfo, kArgumentsCount)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kFunctionTemplateInfo
                         MachineType::IntPtr())     // kArgumentsCount
  DECLARE_DESCRIPTOR(CallFunctionTemplateDescriptor)

  static constexpr inline auto registers();
};

class CallWithSpreadDescriptor
    : public StaticCallInterfaceDescriptor<CallWithSpreadDescriptor> {
 public:
  DEFINE_PARAMETERS_VARARGS(kTarget, kArgumentsCount, kSpread)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kTarget
                         MachineType::Int32(),      // kArgumentsCount
                         MachineType::AnyTagged())  // kSpread
  DECLARE_DESCRIPTOR(CallWithSpreadDescriptor)

  static constexpr inline auto registers();
};

class CallWithSpread_BaselineDescriptor
    : public StaticCallInterfaceDescriptor<CallWithSpread_BaselineDescriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT_VARARGS(kTarget, kArgumentsCount, kSpread, kSlot)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kTarget
                         MachineType::Int32(),      // kArgumentsCount
                         MachineType::AnyTagged(),  // kSpread
                         MachineType::UintPtr())    // kSlot
  DECLARE_DESCRIPTOR(CallWithSpread_BaselineDescriptor)
};

class CallWithSpread_WithFeedbackDescriptor
    : public StaticCallInterfaceDescriptor<
          CallWithSpread_WithFeedbackDescriptor> {
 public:
  DEFINE_PARAMETERS_VARARGS(kTarget, kArgumentsCount, kSpread, kSlot,
                            kFeedbackVector, kReceiver)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kTarget
                         MachineType::Int32(),      // kArgumentsCount
                         MachineType::AnyTagged(),  // kSpread
                         MachineType::UintPtr(),    // kSlot
                         MachineType::AnyTagged(),  // kFeedbackVector
                         MachineType::AnyTagged())  // kReceiver
  DECLARE_DESCRIPTOR(CallWithSpread_WithFeedbackDescriptor)
};

class CallWithArrayLikeDescriptor
    : public StaticCallInterfaceDescriptor<CallWithArrayLikeDescriptor> {
 public:
  DEFINE_PARAMETERS(kTarget, kArgumentsList)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kTarget
                         MachineType::AnyTagged())  // kArgumentsList
  DECLARE_DESCRIPTOR(CallWithArrayLikeDescriptor)

  static constexpr inline auto registers();
};

class CallWithArrayLike_WithFeedbackDescriptor
    : public StaticCallInterfaceDescriptor<
          CallWithArrayLike_WithFeedbackDescriptor> {
 public:
  DEFINE_PARAMETERS(kTarget, kArgumentsList, kSlot, kFeedbackVector, kReceiver)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kTarget
                         MachineType::AnyTagged(),  // kArgumentsList
                         MachineType::UintPtr(),    // kSlot
                         MachineType::AnyTagged(),  // kFeedbackVector
                         MachineType::AnyTagged())  // kReceiver
  DECLARE_DESCRIPTOR(CallWithArrayLike_WithFeedbackDescriptor)
};

class ConstructVarargsDescriptor
    : public StaticCallInterfaceDescriptor<ConstructVarargsDescriptor> {
 public:
  DEFINE_JS_PARAMETERS(kArgumentsLength, kArgumentsList)
  DEFINE_JS_PARAMETER_TYPES(MachineType::Int32(),      // kArgumentsLength
                            MachineType::AnyTagged())  // kArgumentsList

  DECLARE_DESCRIPTOR(ConstructVarargsDescriptor)

  static constexpr inline auto registers();
};

class ConstructForwardVarargsDescriptor
    : public StaticCallInterfaceDescriptor<ConstructForwardVarargsDescriptor> {
 public:
  DEFINE_JS_PARAMETERS(kStartIndex)
  DEFINE_JS_PARAMETER_TYPES(MachineType::Int32())
  DECLARE_DESCRIPTOR(ConstructForwardVarargsDescriptor)

  static constexpr inline auto registers();
};

class ConstructWithSpreadDescriptor
    : public StaticCallInterfaceDescriptor<ConstructWithSpreadDescriptor> {
 public:
  DEFINE_JS_PARAMETERS(kSpread)
  DEFINE_JS_PARAMETER_TYPES(MachineType::AnyTagged())
  DECLARE_DESCRIPTOR(ConstructWithSpreadDescriptor)

  static constexpr inline auto registers();
};

class ConstructWithSpread_BaselineDescriptor
    : public StaticCallInterfaceDescriptor<
          ConstructWithSpread_BaselineDescriptor> {
 public:
  // Note: kSlot comes before kSpread since as an untagged value it must be
  // passed in a register.
  DEFINE_JS_PARAMETERS_NO_CONTEXT(kSlot, kSpread)
  DEFINE_JS_PARAMETER_TYPES(MachineType::UintPtr(),    // kSlot
                            MachineType::AnyTagged())  // kSpread
  DECLARE_DESCRIPTOR(ConstructWithSpread_BaselineDescriptor)
};

class ConstructWithSpread_WithFeedbackDescriptor
    : public StaticCallInterfaceDescriptor<
          ConstructWithSpread_WithFeedbackDescriptor> {
 public:
  // Note: kSlot comes before kSpread since as an untagged value it must be
  // passed in a register.
  DEFINE_JS_PARAMETERS(kSlot, kSpread, kFeedbackVector)
  DEFINE_JS_PARAMETER_TYPES(MachineType::UintPtr(),    // kSlot
                            MachineType::AnyTagged(),  // kSpread
                            MachineType::AnyTagged())  // kFeedbackVector
  DECLARE_DESCRIPTOR(ConstructWithSpread_WithFeedbackDescriptor)
};

class ConstructWithArrayLikeDescriptor
    : public StaticCallInterfaceDescriptor<ConstructWithArrayLikeDescriptor> {
 public:
  DEFINE_PARAMETERS(kTarget, kNewTarget, kArgumentsList)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kTarget
                         MachineType::AnyTagged(),  // kNewTarget
                         MachineType::AnyTagged())  // kArgumentsList
  DECLARE_DESCRIPTOR(ConstructWithArrayLikeDescriptor)

  static constexpr inline auto registers();
};

class ConstructWithArrayLike_WithFeedbackDescriptor
    : public StaticCallInterfaceDescriptor<
          ConstructWithArrayLike_WithFeedbackDescriptor> {
 public:
  DEFINE_PARAMETERS(kTarget, kNewTarget, kArgumentsList, kSlot, kFeedbackVector)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kTarget
                         MachineType::AnyTagged(),  // kNewTarget
                         MachineType::AnyTagged(),  // kArgumentsList
                         MachineType::UintPtr(),    // kSlot
                         MachineType::AnyTagged())  // kFeedbackVector
  DECLARE_DESCRIPTOR(ConstructWithArrayLike_WithFeedbackDescriptor)
};

// TODO(ishell): consider merging this with ArrayConstructorDescriptor
class ConstructStubDescriptor
    : public StaticCallInterfaceDescriptor<ConstructStubDescriptor> {
 public:
  // TODO(jgruber): Remove the unused allocation site parameter.
  DEFINE_JS_PARAMETERS(kAllocationSite)
  DEFINE_JS_PARAMETER_TYPES(MachineType::AnyTagged())

  // TODO(ishell): Use DECLARE_JS_COMPATIBLE_DESCRIPTOR if registers match
  DECLARE_DESCRIPTOR(ConstructStubDescriptor)

  static constexpr inline auto registers();
};

class AbortDescriptor : public StaticCallInterfaceDescriptor<AbortDescriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kMessageOrMessageId)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged())
  DECLARE_DESCRIPTOR(AbortDescriptor)

  static constexpr inline auto registers();
};

class ArrayConstructorDescriptor
    : public StaticJSCallInterfaceDescriptor<ArrayConstructorDescriptor> {
 public:
  DEFINE_JS_PARAMETERS(kAllocationSite)
  DEFINE_JS_PARAMETER_TYPES(MachineType::AnyTagged())

  DECLARE_JS_COMPATIBLE_DESCRIPTOR(ArrayConstructorDescriptor)
};

class ArrayNArgumentsConstructorDescriptor
    : public StaticCallInterfaceDescriptor<
          ArrayNArgumentsConstructorDescriptor> {
 public:
  // This descriptor declares only register arguments while respective number
  // of JS arguments stay on the expression stack.
  // The ArrayNArgumentsConstructor builtin does not access stack arguments
  // directly it just forwards them to the runtime function.
  DEFINE_PARAMETERS(kFunction, kAllocationSite, kActualArgumentsCount)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kFunction,
                         MachineType::AnyTagged(),  // kAllocationSite
                         MachineType::Int32())      // kActualArgumentsCount
  DECLARE_DESCRIPTOR(ArrayNArgumentsConstructorDescriptor)

  static constexpr auto registers();
};

class ArrayNoArgumentConstructorDescriptor
    : public StaticCallInterfaceDescriptor<
          ArrayNoArgumentConstructorDescriptor> {
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
  DECLARE_DESCRIPTOR(ArrayNoArgumentConstructorDescriptor)

  static constexpr auto registers();
};

class ArraySingleArgumentConstructorDescriptor
    : public StaticCallInterfaceDescriptor<
          ArraySingleArgumentConstructorDescriptor> {
 public:
  // This descriptor declares same register arguments as the parent
  // ArrayNArgumentsConstructorDescriptor and it declares indices for
  // JS arguments passed on the expression stack.
  DEFINE_PARAMETERS(kFunction, kAllocationSite, kActualArgumentsCount,
                    kArraySizeSmiParameter, kReceiverParameter)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kFunction
                         MachineType::AnyTagged(),  // kAllocationSite
                         MachineType::Int32(),      // kActualArgumentsCount
                         // JS arguments on the stack
                         MachineType::AnyTagged(),  // kArraySizeSmiParameter
                         MachineType::AnyTagged())  // kReceiverParameter
  DECLARE_DESCRIPTOR(ArraySingleArgumentConstructorDescriptor)

  static constexpr auto registers();
};

class CompareDescriptor
    : public StaticCallInterfaceDescriptor<CompareDescriptor> {
 public:
  DEFINE_PARAMETERS(kLeft, kRight)
  DECLARE_DESCRIPTOR(CompareDescriptor)

  static constexpr inline auto registers();
};

class BinaryOpDescriptor
    : public StaticCallInterfaceDescriptor<BinaryOpDescriptor> {
 public:
  DEFINE_PARAMETERS(kLeft, kRight)
  DECLARE_DESCRIPTOR(BinaryOpDescriptor)

  static constexpr inline auto registers();
};

class BinaryOp_BaselineDescriptor
    : public StaticCallInterfaceDescriptor<BinaryOp_BaselineDescriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kLeft, kRight, kSlot)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kLeft
                         MachineType::AnyTagged(),  // kRight
                         MachineType::UintPtr())    // kSlot
  DECLARE_DESCRIPTOR(BinaryOp_BaselineDescriptor)

  static constexpr inline auto registers();
};

// This desciptor is shared among String.p.charAt/charCodeAt/codePointAt
// as they all have the same interface.
class StringAtDescriptor final
    : public StaticCallInterfaceDescriptor<StringAtDescriptor> {
 public:
  DEFINE_PARAMETERS(kReceiver, kPosition)
  // TODO(turbofan): Return untagged value here.
  DEFINE_RESULT_AND_PARAMETER_TYPES(MachineType::TaggedSigned(),  // result 1
                                    MachineType::AnyTagged(),     // kReceiver
                                    MachineType::IntPtr())        // kPosition
  DECLARE_DESCRIPTOR(StringAtDescriptor)
};

class StringAtAsStringDescriptor final
    : public StaticCallInterfaceDescriptor<StringAtAsStringDescriptor> {
 public:
  DEFINE_PARAMETERS(kReceiver, kPosition)
  // TODO(turbofan): Return untagged value here.
  DEFINE_RESULT_AND_PARAMETER_TYPES(
      MachineType::TaggedPointer(),  // result string
      MachineType::AnyTagged(),      // kReceiver
      MachineType::IntPtr())         // kPosition
  DECLARE_DESCRIPTOR(StringAtAsStringDescriptor)
};

class StringSubstringDescriptor final
    : public StaticCallInterfaceDescriptor<StringSubstringDescriptor> {
 public:
  DEFINE_PARAMETERS(kString, kFrom, kTo)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kString
                         MachineType::IntPtr(),     // kFrom
                         MachineType::IntPtr())     // kTo

  // TODO(turbofan): Allow builtins to return untagged values.
  DECLARE_DESCRIPTOR(StringSubstringDescriptor)
};

class CppBuiltinAdaptorDescriptor
    : public StaticJSCallInterfaceDescriptor<CppBuiltinAdaptorDescriptor> {
 public:
  DEFINE_JS_PARAMETERS(kCFunction)
  DEFINE_JS_PARAMETER_TYPES(MachineType::Pointer())
  DECLARE_JS_COMPATIBLE_DESCRIPTOR(CppBuiltinAdaptorDescriptor)
};

class CEntry1ArgvOnStackDescriptor
    : public StaticCallInterfaceDescriptor<CEntry1ArgvOnStackDescriptor> {
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
  DECLARE_DESCRIPTOR(CEntry1ArgvOnStackDescriptor)

  static constexpr auto registers();
};

class ApiCallbackDescriptor
    : public StaticCallInterfaceDescriptor<ApiCallbackDescriptor> {
 public:
  DEFINE_PARAMETERS_VARARGS(kApiFunctionAddress, kActualArgumentsCount,
                            kCallData, kHolder)
  //                           receiver is implicit stack argument 1
  //                           argv are implicit stack arguments [2, 2 + kArgc[
  DEFINE_PARAMETER_TYPES(MachineType::Pointer(),    // kApiFunctionAddress
                         MachineType::IntPtr(),     // kActualArgumentsCount
                         MachineType::AnyTagged(),  // kCallData
                         MachineType::AnyTagged())  // kHolder
  DECLARE_DESCRIPTOR(ApiCallbackDescriptor)

  static constexpr inline auto registers();
};

class ApiGetterDescriptor
    : public StaticCallInterfaceDescriptor<ApiGetterDescriptor> {
 public:
  DEFINE_PARAMETERS(kReceiver, kHolder, kCallback)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kReceiver
                         MachineType::AnyTagged(),  // kHolder
                         MachineType::AnyTagged())  // kCallback
  DECLARE_DESCRIPTOR(ApiGetterDescriptor)

  static constexpr inline Register ReceiverRegister();
  static constexpr inline Register HolderRegister();
  static constexpr inline Register CallbackRegister();

  static constexpr auto registers();
};

// TODO(turbofan): We should probably rename this to GrowFastElementsDescriptor.
class GrowArrayElementsDescriptor
    : public StaticCallInterfaceDescriptor<GrowArrayElementsDescriptor> {
 public:
  DEFINE_PARAMETERS(kObject, kKey)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kObject
                         MachineType::AnyTagged())  // kKey
  DECLARE_DESCRIPTOR(GrowArrayElementsDescriptor)

  static constexpr inline Register ObjectRegister();
  static constexpr inline Register KeyRegister();

  static constexpr auto registers();
};

class BaselineOutOfLinePrologueDescriptor
    : public StaticCallInterfaceDescriptor<
          BaselineOutOfLinePrologueDescriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kCalleeContext, kClosure,
                               kJavaScriptCallArgCount, kStackFrameSize,
                               kJavaScriptCallNewTarget,
                               kInterpreterBytecodeArray)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kCalleeContext
                         MachineType::AnyTagged(),  // kClosure
                         MachineType::Int32(),      // kJavaScriptCallArgCount
                         MachineType::Int32(),      // kStackFrameSize
                         MachineType::AnyTagged(),  // kJavaScriptCallNewTarget
                         MachineType::AnyTagged())  // kInterpreterBytecodeArray
  DECLARE_DESCRIPTOR(BaselineOutOfLinePrologueDescriptor)

  static constexpr inline auto registers();

  // We pass the context manually, so we have one extra register.
  static constexpr int kMaxRegisterParams =
      StaticCallInterfaceDescriptor::kMaxRegisterParams + 1;
};

class BaselineLeaveFrameDescriptor
    : public StaticCallInterfaceDescriptor<BaselineLeaveFrameDescriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kParamsSize, kWeight)
  DEFINE_PARAMETER_TYPES(MachineType::Int32(),  // kParamsSize
                         MachineType::Int32())  // kWeight
  DECLARE_DESCRIPTOR(BaselineLeaveFrameDescriptor)

  static constexpr inline Register ParamsSizeRegister();
  static constexpr inline Register WeightRegister();

  static constexpr inline auto registers();
};

class V8_EXPORT_PRIVATE InterpreterDispatchDescriptor
    : public StaticCallInterfaceDescriptor<InterpreterDispatchDescriptor> {
 public:
  DEFINE_PARAMETERS(kAccumulator, kBytecodeOffset, kBytecodeArray,
                    kDispatchTable)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kAccumulator
                         MachineType::IntPtr(),     // kBytecodeOffset
                         MachineType::AnyTagged(),  // kBytecodeArray
                         MachineType::IntPtr())     // kDispatchTable
  DECLARE_DESCRIPTOR(InterpreterDispatchDescriptor)

  static constexpr inline auto registers();
};

class InterpreterPushArgsThenCallDescriptor
    : public StaticCallInterfaceDescriptor<
          InterpreterPushArgsThenCallDescriptor> {
 public:
  DEFINE_PARAMETERS(kNumberOfArguments, kFirstArgument, kFunction)
  DEFINE_PARAMETER_TYPES(MachineType::Int32(),      // kNumberOfArguments
                         MachineType::Pointer(),    // kFirstArgument
                         MachineType::AnyTagged())  // kFunction
  DECLARE_DESCRIPTOR(InterpreterPushArgsThenCallDescriptor)

  static constexpr inline auto registers();
};

class InterpreterPushArgsThenConstructDescriptor
    : public StaticCallInterfaceDescriptor<
          InterpreterPushArgsThenConstructDescriptor> {
 public:
  DEFINE_PARAMETERS(kNumberOfArguments, kFirstArgument, kConstructor,
                    kNewTarget, kFeedbackElement)
  DEFINE_PARAMETER_TYPES(MachineType::Int32(),      // kNumberOfArguments
                         MachineType::Pointer(),    // kFirstArgument
                         MachineType::AnyTagged(),  // kConstructor
                         MachineType::AnyTagged(),  // kNewTarget
                         MachineType::AnyTagged())  // kFeedbackElement
  DECLARE_DESCRIPTOR(InterpreterPushArgsThenConstructDescriptor)

  static constexpr inline auto registers();
};

class InterpreterCEntry1Descriptor
    : public StaticCallInterfaceDescriptor<InterpreterCEntry1Descriptor> {
 public:
  DEFINE_RESULT_AND_PARAMETERS(1, kNumberOfArguments, kFirstArgument,
                               kFunctionEntry)
  DEFINE_RESULT_AND_PARAMETER_TYPES(MachineType::AnyTagged(),  // result 1
                                    MachineType::Int32(),  // kNumberOfArguments
                                    MachineType::Pointer(),  // kFirstArgument
                                    MachineType::Pointer())  // kFunctionEntry
  DECLARE_DESCRIPTOR(InterpreterCEntry1Descriptor)

  static constexpr auto registers();
};

class InterpreterCEntry2Descriptor
    : public StaticCallInterfaceDescriptor<InterpreterCEntry2Descriptor> {
 public:
  DEFINE_RESULT_AND_PARAMETERS(2, kNumberOfArguments, kFirstArgument,
                               kFunctionEntry)
  DEFINE_RESULT_AND_PARAMETER_TYPES(MachineType::AnyTagged(),  // result 1
                                    MachineType::AnyTagged(),  // result 2
                                    MachineType::Int32(),  // kNumberOfArguments
                                    MachineType::Pointer(),  // kFirstArgument
                                    MachineType::Pointer())  // kFunctionEntry
  DECLARE_DESCRIPTOR(InterpreterCEntry2Descriptor)

  static constexpr auto registers();
};

class ForInPrepareDescriptor
    : public StaticCallInterfaceDescriptor<ForInPrepareDescriptor> {
 public:
  DEFINE_RESULT_AND_PARAMETERS(2, kEnumerator, kVectorIndex, kFeedbackVector)
  DEFINE_RESULT_AND_PARAMETER_TYPES(
      MachineType::AnyTagged(),     // result 1 (cache array)
      MachineType::AnyTagged(),     // result 2 (cache length)
      MachineType::AnyTagged(),     // kEnumerator
      MachineType::TaggedSigned(),  // kVectorIndex
      MachineType::AnyTagged())     // kFeedbackVector
  DECLARE_DESCRIPTOR(ForInPrepareDescriptor)
};

class ResumeGeneratorDescriptor final
    : public StaticCallInterfaceDescriptor<ResumeGeneratorDescriptor> {
 public:
  DEFINE_PARAMETERS(kValue, kGenerator)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kValue
                         MachineType::AnyTagged())  // kGenerator
  DECLARE_DESCRIPTOR(ResumeGeneratorDescriptor)

  static constexpr inline auto registers();
};

class ResumeGeneratorBaselineDescriptor final
    : public StaticCallInterfaceDescriptor<ResumeGeneratorBaselineDescriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kGeneratorObject, kRegisterCount)
  DEFINE_RESULT_AND_PARAMETER_TYPES(
      MachineType::TaggedSigned(),  // return type
      MachineType::AnyTagged(),     // kGeneratorObject
      MachineType::IntPtr(),        // kRegisterCount
  )
  DECLARE_DESCRIPTOR(ResumeGeneratorBaselineDescriptor)
};

class SuspendGeneratorBaselineDescriptor final
    : public StaticCallInterfaceDescriptor<SuspendGeneratorBaselineDescriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kGeneratorObject, kSuspendId, kBytecodeOffset,
                               kRegisterCount)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kGeneratorObject
                         MachineType::IntPtr(),     // kSuspendId
                         MachineType::IntPtr(),     // kBytecodeOffset
                         MachineType::IntPtr(),     // kRegisterCount
  )
  DECLARE_DESCRIPTOR(SuspendGeneratorBaselineDescriptor)
};

class RunMicrotasksEntryDescriptor final
    : public StaticCallInterfaceDescriptor<RunMicrotasksEntryDescriptor> {
 public:
  DEFINE_PARAMETERS_ENTRY(kRootRegisterValue, kMicrotaskQueue)
  DEFINE_PARAMETER_TYPES(MachineType::Pointer(),  // kRootRegisterValue
                         MachineType::Pointer())  // kMicrotaskQueue
  DECLARE_DESCRIPTOR(RunMicrotasksEntryDescriptor)

  static constexpr inline auto registers();
};

class RunMicrotasksDescriptor final
    : public StaticCallInterfaceDescriptor<RunMicrotasksDescriptor> {
 public:
  DEFINE_PARAMETERS(kMicrotaskQueue)
  DEFINE_PARAMETER_TYPES(MachineType::Pointer())
  DECLARE_DESCRIPTOR(RunMicrotasksDescriptor)

  static constexpr inline Register MicrotaskQueueRegister();
};

class WasmFloat32ToNumberDescriptor final
    : public StaticCallInterfaceDescriptor<WasmFloat32ToNumberDescriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kValue)
  DEFINE_RESULT_AND_PARAMETER_TYPES(MachineType::AnyTagged(),  // result
                                    MachineType::Float32())    // value
  DECLARE_DESCRIPTOR(WasmFloat32ToNumberDescriptor)

#if V8_TARGET_ARCH_IA32
  // We need a custom descriptor on ia32 to avoid using xmm0.
  static constexpr inline auto registers();
#endif
};

class WasmFloat64ToNumberDescriptor final
    : public StaticCallInterfaceDescriptor<WasmFloat64ToNumberDescriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kValue)
  DEFINE_RESULT_AND_PARAMETER_TYPES(MachineType::AnyTagged(),  // result
                                    MachineType::Float64())    // value
  DECLARE_DESCRIPTOR(WasmFloat64ToNumberDescriptor)

#if V8_TARGET_ARCH_IA32
  // We need a custom descriptor on ia32 to avoid using xmm0.
  static constexpr inline auto registers();
#endif
};

class V8_EXPORT_PRIVATE I64ToBigIntDescriptor final
    : public StaticCallInterfaceDescriptor<I64ToBigIntDescriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kArgument)
  DEFINE_PARAMETER_TYPES(MachineType::Int64())  // kArgument
  DECLARE_DESCRIPTOR(I64ToBigIntDescriptor)
};

// 32 bits version of the I64ToBigIntDescriptor call interface descriptor
class V8_EXPORT_PRIVATE I32PairToBigIntDescriptor final
    : public StaticCallInterfaceDescriptor<I32PairToBigIntDescriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kLow, kHigh)
  DEFINE_PARAMETER_TYPES(MachineType::Uint32(),  // kLow
                         MachineType::Uint32())  // kHigh
  DECLARE_DESCRIPTOR(I32PairToBigIntDescriptor)
};

class V8_EXPORT_PRIVATE BigIntToI64Descriptor final
    : public StaticCallInterfaceDescriptor<BigIntToI64Descriptor> {
 public:
  DEFINE_PARAMETERS(kArgument)
  DEFINE_RESULT_AND_PARAMETER_TYPES(MachineType::Int64(),      // result 1
                                    MachineType::AnyTagged())  // kArgument
  DECLARE_DESCRIPTOR(BigIntToI64Descriptor)
};

class V8_EXPORT_PRIVATE BigIntToI32PairDescriptor final
    : public StaticCallInterfaceDescriptor<BigIntToI32PairDescriptor> {
 public:
  DEFINE_RESULT_AND_PARAMETERS(2, kArgument)
  DEFINE_RESULT_AND_PARAMETER_TYPES(MachineType::Uint32(),     // result 1
                                    MachineType::Uint32(),     // result 2
                                    MachineType::AnyTagged())  // kArgument
  DECLARE_DESCRIPTOR(BigIntToI32PairDescriptor)
};

class WasmI32AtomicWait32Descriptor final
    : public StaticCallInterfaceDescriptor<WasmI32AtomicWait32Descriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kAddress, kExpectedValue, kTimeoutLow,
                               kTimeoutHigh)
  DEFINE_RESULT_AND_PARAMETER_TYPES(MachineType::Uint32(),  // result 1
                                    MachineType::Uint32(),  // kAddress
                                    MachineType::Int32(),   // kExpectedValue
                                    MachineType::Uint32(),  // kTimeoutLow
                                    MachineType::Uint32())  // kTimeoutHigh
  DECLARE_DESCRIPTOR(WasmI32AtomicWait32Descriptor)
};

class WasmI64AtomicWait32Descriptor final
    : public StaticCallInterfaceDescriptor<WasmI64AtomicWait32Descriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kAddress, kExpectedValueLow, kExpectedValueHigh,
                               kTimeoutLow, kTimeoutHigh)

  static constexpr bool kNoStackScan = true;

  DEFINE_RESULT_AND_PARAMETER_TYPES(
      MachineType::Uint32(),  // result 1
      MachineType::Uint32(),  // kAddress
      MachineType::Uint32(),  // kExpectedValueLow
      MachineType::Uint32(),  // kExpectedValueHigh
      MachineType::Uint32(),  // kTimeoutLow
      MachineType::Uint32())  // kTimeoutHigh

  DECLARE_DESCRIPTOR(WasmI64AtomicWait32Descriptor)
};

class CloneObjectWithVectorDescriptor final
    : public StaticCallInterfaceDescriptor<CloneObjectWithVectorDescriptor> {
 public:
  DEFINE_PARAMETERS(kSource, kFlags, kSlot, kVector)
  DEFINE_RESULT_AND_PARAMETER_TYPES(MachineType::TaggedPointer(),  // result 1
                                    MachineType::AnyTagged(),      // kSource
                                    MachineType::TaggedSigned(),   // kFlags
                                    MachineType::TaggedSigned(),   // kSlot
                                    MachineType::AnyTagged())      // kVector
  DECLARE_DESCRIPTOR(CloneObjectWithVectorDescriptor)
};

class CloneObjectBaselineDescriptor final
    : public StaticCallInterfaceDescriptor<CloneObjectBaselineDescriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kSource, kFlags, kSlot)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),     // kSource
                         MachineType::TaggedSigned(),  // kFlags
                         MachineType::TaggedSigned())  // kSlot
  DECLARE_DESCRIPTOR(CloneObjectBaselineDescriptor)
};

class BinaryOp_WithFeedbackDescriptor
    : public StaticCallInterfaceDescriptor<BinaryOp_WithFeedbackDescriptor> {
 public:
  DEFINE_PARAMETERS(kLeft, kRight, kSlot, kFeedbackVector)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kLeft
                         MachineType::AnyTagged(),  // kRight
                         MachineType::UintPtr(),    // kSlot
                         MachineType::AnyTagged())  // kFeedbackVector
  DECLARE_DESCRIPTOR(BinaryOp_WithFeedbackDescriptor)
};

class CallTrampoline_Baseline_CompactDescriptor
    : public StaticCallInterfaceDescriptor<
          CallTrampoline_Baseline_CompactDescriptor> {
 public:
  using ArgumentCountField = base::BitField<uint32_t, 0, 8>;
  using SlotField = base::BitField<uintptr_t, 8, 24>;

  static bool EncodeBitField(uint32_t argc, uintptr_t slot, uint32_t* out) {
    if (ArgumentCountField::is_valid(argc) && SlotField::is_valid(slot)) {
      *out = ArgumentCountField::encode(argc) | SlotField::encode(slot);
      return true;
    }
    return false;
  }

  DEFINE_PARAMETERS_NO_CONTEXT_VARARGS(kFunction, kBitField)
  DEFINE_PARAMETER_TYPES(
      MachineType::AnyTagged(),  // kFunction
      MachineType::Uint32())     // kBitField = ArgumentCountField | SlotField
  DECLARE_DESCRIPTOR(CallTrampoline_Baseline_CompactDescriptor)
};

class CallTrampoline_BaselineDescriptor
    : public StaticCallInterfaceDescriptor<CallTrampoline_BaselineDescriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT_VARARGS(kFunction, kActualArgumentsCount, kSlot)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kFunction
                         MachineType::Int32(),      // kActualArgumentsCount
                         MachineType::UintPtr())    // kSlot
  DECLARE_DESCRIPTOR(CallTrampoline_BaselineDescriptor)
};

class CallTrampoline_WithFeedbackDescriptor
    : public StaticCallInterfaceDescriptor<
          CallTrampoline_WithFeedbackDescriptor> {
 public:
  DEFINE_PARAMETERS_VARARGS(kFunction, kActualArgumentsCount, kSlot,
                            kFeedbackVector, kReceiver)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kFunction
                         MachineType::Int32(),      // kActualArgumentsCount
                         MachineType::UintPtr(),    // kSlot
                         MachineType::AnyTagged(),  // kFeedbackVector
                         MachineType::AnyTagged())  // kReceiver
  DECLARE_DESCRIPTOR(CallTrampoline_WithFeedbackDescriptor)
};

class Compare_WithFeedbackDescriptor
    : public StaticCallInterfaceDescriptor<Compare_WithFeedbackDescriptor> {
 public:
  DEFINE_PARAMETERS(kLeft, kRight, kSlot, kFeedbackVector)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kLeft
                         MachineType::AnyTagged(),  // kRight
                         MachineType::UintPtr(),    // kSlot
                         MachineType::AnyTagged())  // kFeedbackVector
  DECLARE_DESCRIPTOR(Compare_WithFeedbackDescriptor)
};

class Compare_BaselineDescriptor
    : public StaticCallInterfaceDescriptor<Compare_BaselineDescriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kLeft, kRight, kSlot)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kLeft
                         MachineType::AnyTagged(),  // kRight
                         MachineType::UintPtr())    // kSlot
  DECLARE_DESCRIPTOR(Compare_BaselineDescriptor)

  static constexpr inline auto registers();
};

class Construct_BaselineDescriptor
    : public StaticJSCallInterfaceDescriptor<Construct_BaselineDescriptor> {
 public:
  DEFINE_JS_PARAMETERS_NO_CONTEXT(kSlot)
  DEFINE_JS_PARAMETER_TYPES(MachineType::UintPtr())  // kSlot
  DECLARE_JS_COMPATIBLE_DESCRIPTOR(Construct_BaselineDescriptor)
};

class Construct_WithFeedbackDescriptor
    : public StaticJSCallInterfaceDescriptor<Construct_WithFeedbackDescriptor> {
 public:
  // kSlot is passed in a register, kFeedbackVector on the stack.
  DEFINE_JS_PARAMETERS(kSlot, kFeedbackVector)
  DEFINE_JS_PARAMETER_TYPES(MachineType::UintPtr(),    // kSlot
                            MachineType::AnyTagged())  // kFeedbackVector
  DECLARE_JS_COMPATIBLE_DESCRIPTOR(Construct_WithFeedbackDescriptor)
};

class UnaryOp_WithFeedbackDescriptor
    : public StaticCallInterfaceDescriptor<UnaryOp_WithFeedbackDescriptor> {
 public:
  DEFINE_PARAMETERS(kValue, kSlot, kFeedbackVector)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kValue
                         MachineType::UintPtr(),    // kSlot
                         MachineType::AnyTagged())  // kFeedbackVector
  DECLARE_DESCRIPTOR(UnaryOp_WithFeedbackDescriptor)
};

class UnaryOp_BaselineDescriptor
    : public StaticCallInterfaceDescriptor<UnaryOp_BaselineDescriptor> {
 public:
  DEFINE_PARAMETERS_NO_CONTEXT(kValue, kSlot)
  DEFINE_PARAMETER_TYPES(MachineType::AnyTagged(),  // kValue
                         MachineType::UintPtr())    // kSlot
  DECLARE_DESCRIPTOR(UnaryOp_BaselineDescriptor)
};

#define DEFINE_TFS_BUILTIN_DESCRIPTOR(Name, ...)                 \
  class Name##Descriptor                                         \
      : public StaticCallInterfaceDescriptor<Name##Descriptor> { \
   public:                                                       \
    DEFINE_PARAMETERS(__VA_ARGS__)                               \
    DECLARE_DEFAULT_DESCRIPTOR(Name##Descriptor)                 \
  };
BUILTIN_LIST_TFS(DEFINE_TFS_BUILTIN_DESCRIPTOR)
#undef DEFINE_TFS_BUILTIN_DESCRIPTOR

// This file contains interface descriptor class definitions for builtins
// defined in Torque. It is included here because the class definitions need to
// precede the definition of name##Descriptor::key() below.
#include "torque-generated/interface-descriptors.inc"

#undef DECLARE_DEFAULT_DESCRIPTOR
#undef DECLARE_DESCRIPTOR_WITH_BASE
#undef DECLARE_DESCRIPTOR
#undef DECLARE_JS_COMPATIBLE_DESCRIPTOR
#undef DEFINE_RESULT_AND_PARAMETERS
#undef DEFINE_PARAMETERS_ENTRY
#undef DEFINE_PARAMETERS
#undef DEFINE_PARAMETERS_VARARGS
#undef DEFINE_PARAMETERS_NO_CONTEXT
#undef DEFINE_RESULT_AND_PARAMETERS_NO_CONTEXT
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

#endif  // V8_CODEGEN_INTERFACE_DESCRIPTORS_H_
