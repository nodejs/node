// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_SIMULATOR_BASE_H_
#define V8_EXECUTION_SIMULATOR_BASE_H_

#include <type_traits>

#include "src/base/hashmap.h"
#include "src/common/globals.h"
#include "src/execution/isolate.h"

#if defined(USE_SIMULATOR)

namespace v8 {
namespace internal {

class Instruction;
class Redirection;

class SimulatorBase {
 public:
  // Call on process start and exit.
  static void InitializeOncePerProcess();
  static void GlobalTearDown();

  static base::Mutex* redirection_mutex() { return redirection_mutex_; }
  static Redirection* redirection() { return redirection_; }
  static void set_redirection(Redirection* r) { redirection_ = r; }

  static base::Mutex* i_cache_mutex() { return i_cache_mutex_; }
  static base::CustomMatcherHashMap* i_cache() { return i_cache_; }

  // Runtime call support.
  static Address RedirectExternalReference(Address external_function,
                                           ExternalReference::Type type);

 protected:
  template <typename Return, typename SimT, typename CallImpl, typename... Args>
  static Return VariadicCall(SimT* sim, CallImpl call, Address entry,
                             Args... args) {
    // Convert all arguments to intptr_t. Fails if any argument is not integral
    // or pointer.
    std::array<intptr_t, sizeof...(args)> args_arr{{ConvertArg(args)...}};
    intptr_t ret = (sim->*call)(entry, args_arr.size(), args_arr.data());
    return ConvertReturn<Return>(ret);
  }

  // Convert back integral return types. This is always a narrowing conversion.
  template <typename T>
  static typename std::enable_if<std::is_integral<T>::value, T>::type
  ConvertReturn(intptr_t ret) {
    static_assert(sizeof(T) <= sizeof(intptr_t), "type bigger than ptrsize");
    return static_cast<T>(ret);
  }

  // Convert back pointer-typed return types.
  template <typename T>
  static typename std::enable_if<std::is_pointer<T>::value, T>::type
  ConvertReturn(intptr_t ret) {
    return reinterpret_cast<T>(ret);
  }

  template <typename T>
  static typename std::enable_if<std::is_base_of<Object, T>::value, T>::type
  ConvertReturn(intptr_t ret) {
    return Object(ret);
  }

  // Convert back void return type (i.e. no return).
  template <typename T>
  static typename std::enable_if<std::is_void<T>::value, T>::type ConvertReturn(
      intptr_t ret) {}

 private:
  static base::Mutex* redirection_mutex_;
  static Redirection* redirection_;

  static base::Mutex* i_cache_mutex_;
  static base::CustomMatcherHashMap* i_cache_;

  // Helper methods to convert arbitrary integer or pointer arguments to the
  // needed generic argument type intptr_t.

  // Convert integral argument to intptr_t.
  template <typename T>
  static typename std::enable_if<std::is_integral<T>::value, intptr_t>::type
  ConvertArg(T arg) {
    static_assert(sizeof(T) <= sizeof(intptr_t), "type bigger than ptrsize");
#if V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_RISCV64
    // The MIPS64 and RISCV64 calling convention is to sign extend all values,
    // even unsigned ones.
    using signed_t = typename std::make_signed<T>::type;
    return static_cast<intptr_t>(static_cast<signed_t>(arg));
#else
    // Standard C++ convertion: Sign-extend signed values, zero-extend unsigned
    // values.
    return static_cast<intptr_t>(arg);
#endif
  }

  // Convert pointer-typed argument to intptr_t.
  template <typename T>
  static typename std::enable_if<std::is_pointer<T>::value, intptr_t>::type
  ConvertArg(T arg) {
    return reinterpret_cast<intptr_t>(arg);
  }
};

// When the generated code calls an external reference we need to catch that in
// the simulator.  The external reference will be a function compiled for the
// host architecture.  We need to call that function instead of trying to
// execute it with the simulator.  We do that by redirecting the external
// reference to a trapping instruction that is handled by the simulator.  We
// write the original destination of the jump just at a known offset from the
// trapping instruction so the simulator knows what to call.
//
// The following are trapping instructions used for various architectures:
//  - V8_TARGET_ARCH_ARM: svc (Supervisor Call)
//  - V8_TARGET_ARCH_ARM64: svc (Supervisor Call)
//  - V8_TARGET_ARCH_MIPS: swi (software-interrupt)
//  - V8_TARGET_ARCH_MIPS64: swi (software-interrupt)
//  - V8_TARGET_ARCH_PPC: svc (Supervisor Call)
//  - V8_TARGET_ARCH_PPC64: svc (Supervisor Call)
//  - V8_TARGET_ARCH_S390: svc (Supervisor Call)
//  - V8_TARGET_ARCH_RISCV64: ecall (Supervisor Call)
class Redirection {
 public:
  Redirection(Address external_function, ExternalReference::Type type);

  Address address_of_instruction() {
#if ABI_USES_FUNCTION_DESCRIPTORS
    return reinterpret_cast<Address>(function_descriptor_);
#else
    return reinterpret_cast<Address>(&instruction_);
#endif
  }

  void* external_function() {
    return reinterpret_cast<void*>(external_function_);
  }
  ExternalReference::Type type() { return type_; }

  static Redirection* Get(Address external_function,
                          ExternalReference::Type type);

  static Redirection* FromInstruction(Instruction* instruction) {
    Address addr_of_instruction = reinterpret_cast<Address>(instruction);
    Address addr_of_redirection =
        addr_of_instruction - offsetof(Redirection, instruction_);
    return reinterpret_cast<Redirection*>(addr_of_redirection);
  }

  static void* ReverseRedirection(intptr_t reg) {
    Redirection* redirection = FromInstruction(
        reinterpret_cast<Instruction*>(reinterpret_cast<void*>(reg)));
    return redirection->external_function();
  }

  static void DeleteChain(Redirection* redirection) {
    while (redirection != nullptr) {
      Redirection* next = redirection->next_;
      delete redirection;
      redirection = next;
    }
  }

 private:
  Address external_function_;
  uint32_t instruction_;
  ExternalReference::Type type_;
  Redirection* next_;
#if ABI_USES_FUNCTION_DESCRIPTORS
  intptr_t function_descriptor_[3];
#endif
};

}  // namespace internal
}  // namespace v8

#endif  // defined(USE_SIMULATOR)
#endif  // V8_EXECUTION_SIMULATOR_BASE_H_
