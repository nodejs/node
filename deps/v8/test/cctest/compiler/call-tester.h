// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CCTEST_COMPILER_CALL_TESTER_H_
#define V8_CCTEST_COMPILER_CALL_TESTER_H_

#include "src/v8.h"

#include "src/simulator.h"

#if V8_TARGET_ARCH_IA32
#if __GNUC__
#define V8_CDECL __attribute__((cdecl))
#else
#define V8_CDECL __cdecl
#endif
#else
#define V8_CDECL
#endif

namespace v8 {
namespace internal {
namespace compiler {

// TODO(titzer): use c-signature.h instead of ReturnValueTraits
template <typename R>
struct ReturnValueTraits {
  static R Cast(uintptr_t r) { return reinterpret_cast<R>(r); }
  static MachineType Representation() {
    // TODO(dcarney): detect when R is of a subclass of Object* instead of this
    // type check.
    while (false) {
      *(static_cast<Object* volatile*>(0)) = static_cast<R>(0);
    }
    return kMachAnyTagged;
  }
};

template <>
struct ReturnValueTraits<int32_t*> {
  static int32_t* Cast(uintptr_t r) { return reinterpret_cast<int32_t*>(r); }
  static MachineType Representation() { return kMachPtr; }
};

template <>
struct ReturnValueTraits<void> {
  static void Cast(uintptr_t r) {}
  static MachineType Representation() { return kMachPtr; }
};

template <>
struct ReturnValueTraits<bool> {
  static bool Cast(uintptr_t r) { return static_cast<bool>(r); }
  static MachineType Representation() { return kRepBit; }
};

template <>
struct ReturnValueTraits<int32_t> {
  static int32_t Cast(uintptr_t r) { return static_cast<int32_t>(r); }
  static MachineType Representation() { return kMachInt32; }
};

template <>
struct ReturnValueTraits<uint32_t> {
  static uint32_t Cast(uintptr_t r) { return static_cast<uint32_t>(r); }
  static MachineType Representation() { return kMachUint32; }
};

template <>
struct ReturnValueTraits<int64_t> {
  static int64_t Cast(uintptr_t r) { return static_cast<int64_t>(r); }
  static MachineType Representation() { return kMachInt64; }
};

template <>
struct ReturnValueTraits<uint64_t> {
  static uint64_t Cast(uintptr_t r) { return static_cast<uint64_t>(r); }
  static MachineType Representation() { return kMachUint64; }
};

template <>
struct ReturnValueTraits<int16_t> {
  static int16_t Cast(uintptr_t r) { return static_cast<int16_t>(r); }
  static MachineType Representation() { return kMachInt16; }
};

template <>
struct ReturnValueTraits<uint16_t> {
  static uint16_t Cast(uintptr_t r) { return static_cast<uint16_t>(r); }
  static MachineType Representation() { return kMachUint16; }
};

template <>
struct ReturnValueTraits<int8_t> {
  static int8_t Cast(uintptr_t r) { return static_cast<int8_t>(r); }
  static MachineType Representation() { return kMachInt8; }
};

template <>
struct ReturnValueTraits<uint8_t> {
  static uint8_t Cast(uintptr_t r) { return static_cast<uint8_t>(r); }
  static MachineType Representation() { return kMachUint8; }
};

template <>
struct ReturnValueTraits<double> {
  static double Cast(uintptr_t r) {
    UNREACHABLE();
    return 0.0;
  }
  static MachineType Representation() { return kMachFloat64; }
};


template <typename R>
struct ParameterTraits {
  static uintptr_t Cast(R r) { return static_cast<uintptr_t>(r); }
};

template <>
struct ParameterTraits<int*> {
  static uintptr_t Cast(int* r) { return reinterpret_cast<uintptr_t>(r); }
};

template <typename T>
struct ParameterTraits<T*> {
  static uintptr_t Cast(void* r) { return reinterpret_cast<uintptr_t>(r); }
};

class CallHelper {
 public:
  explicit CallHelper(Isolate* isolate, MachineSignature* machine_sig)
      : machine_sig_(machine_sig), isolate_(isolate) {
    USE(isolate_);
  }
  virtual ~CallHelper() {}

  static MachineSignature* MakeMachineSignature(
      Zone* zone, MachineType return_type, MachineType p0 = kMachNone,
      MachineType p1 = kMachNone, MachineType p2 = kMachNone,
      MachineType p3 = kMachNone, MachineType p4 = kMachNone) {
    // Count the number of parameters.
    size_t param_count = 5;
    MachineType types[] = {p0, p1, p2, p3, p4};
    while (param_count > 0 && types[param_count - 1] == kMachNone)
      param_count--;
    size_t return_count = return_type == kMachNone ? 0 : 1;

    // Build the machine signature.
    MachineSignature::Builder builder(zone, return_count, param_count);
    if (return_count > 0) builder.AddReturn(return_type);
    for (size_t i = 0; i < param_count; i++) {
      builder.AddParam(types[i]);
    }
    return builder.Build();
  }

 protected:
  MachineSignature* machine_sig_;
  void VerifyParameters(size_t parameter_count, MachineType* parameter_types) {
    CHECK(machine_sig_->parameter_count() == parameter_count);
    for (size_t i = 0; i < parameter_count; i++) {
      CHECK_EQ(machine_sig_->GetParam(i), parameter_types[i]);
    }
  }
  virtual byte* Generate() = 0;

 private:
#if USE_SIMULATOR && V8_TARGET_ARCH_ARM64
  uintptr_t CallSimulator(byte* f, Simulator::CallArgument* args) {
    Simulator* simulator = Simulator::current(isolate_);
    return static_cast<uintptr_t>(simulator->CallInt64(f, args));
  }

  template <typename R, typename F>
  R DoCall(F* f) {
    Simulator::CallArgument args[] = {Simulator::CallArgument::End()};
    return ReturnValueTraits<R>::Cast(CallSimulator(FUNCTION_ADDR(f), args));
  }
  template <typename R, typename F, typename P1>
  R DoCall(F* f, P1 p1) {
    Simulator::CallArgument args[] = {Simulator::CallArgument(p1),
                                      Simulator::CallArgument::End()};
    return ReturnValueTraits<R>::Cast(CallSimulator(FUNCTION_ADDR(f), args));
  }
  template <typename R, typename F, typename P1, typename P2>
  R DoCall(F* f, P1 p1, P2 p2) {
    Simulator::CallArgument args[] = {Simulator::CallArgument(p1),
                                      Simulator::CallArgument(p2),
                                      Simulator::CallArgument::End()};
    return ReturnValueTraits<R>::Cast(CallSimulator(FUNCTION_ADDR(f), args));
  }
  template <typename R, typename F, typename P1, typename P2, typename P3>
  R DoCall(F* f, P1 p1, P2 p2, P3 p3) {
    Simulator::CallArgument args[] = {
        Simulator::CallArgument(p1), Simulator::CallArgument(p2),
        Simulator::CallArgument(p3), Simulator::CallArgument::End()};
    return ReturnValueTraits<R>::Cast(CallSimulator(FUNCTION_ADDR(f), args));
  }
  template <typename R, typename F, typename P1, typename P2, typename P3,
            typename P4>
  R DoCall(F* f, P1 p1, P2 p2, P3 p3, P4 p4) {
    Simulator::CallArgument args[] = {
        Simulator::CallArgument(p1), Simulator::CallArgument(p2),
        Simulator::CallArgument(p3), Simulator::CallArgument(p4),
        Simulator::CallArgument::End()};
    return ReturnValueTraits<R>::Cast(CallSimulator(FUNCTION_ADDR(f), args));
  }
#elif USE_SIMULATOR && V8_TARGET_ARCH_ARM
  uintptr_t CallSimulator(byte* f, int32_t p1 = 0, int32_t p2 = 0,
                          int32_t p3 = 0, int32_t p4 = 0) {
    Simulator* simulator = Simulator::current(isolate_);
    return static_cast<uintptr_t>(simulator->Call(f, 4, p1, p2, p3, p4));
  }
  template <typename R, typename F>
  R DoCall(F* f) {
    return ReturnValueTraits<R>::Cast(CallSimulator(FUNCTION_ADDR(f)));
  }
  template <typename R, typename F, typename P1>
  R DoCall(F* f, P1 p1) {
    return ReturnValueTraits<R>::Cast(
        CallSimulator(FUNCTION_ADDR(f), ParameterTraits<P1>::Cast(p1)));
  }
  template <typename R, typename F, typename P1, typename P2>
  R DoCall(F* f, P1 p1, P2 p2) {
    return ReturnValueTraits<R>::Cast(
        CallSimulator(FUNCTION_ADDR(f), ParameterTraits<P1>::Cast(p1),
                      ParameterTraits<P2>::Cast(p2)));
  }
  template <typename R, typename F, typename P1, typename P2, typename P3>
  R DoCall(F* f, P1 p1, P2 p2, P3 p3) {
    return ReturnValueTraits<R>::Cast(CallSimulator(
        FUNCTION_ADDR(f), ParameterTraits<P1>::Cast(p1),
        ParameterTraits<P2>::Cast(p2), ParameterTraits<P3>::Cast(p3)));
  }
  template <typename R, typename F, typename P1, typename P2, typename P3,
            typename P4>
  R DoCall(F* f, P1 p1, P2 p2, P3 p3, P4 p4) {
    return ReturnValueTraits<R>::Cast(CallSimulator(
        FUNCTION_ADDR(f), ParameterTraits<P1>::Cast(p1),
        ParameterTraits<P2>::Cast(p2), ParameterTraits<P3>::Cast(p3),
        ParameterTraits<P4>::Cast(p4)));
  }
#else
  template <typename R, typename F>
  R DoCall(F* f) {
    return f();
  }
  template <typename R, typename F, typename P1>
  R DoCall(F* f, P1 p1) {
    return f(p1);
  }
  template <typename R, typename F, typename P1, typename P2>
  R DoCall(F* f, P1 p1, P2 p2) {
    return f(p1, p2);
  }
  template <typename R, typename F, typename P1, typename P2, typename P3>
  R DoCall(F* f, P1 p1, P2 p2, P3 p3) {
    return f(p1, p2, p3);
  }
  template <typename R, typename F, typename P1, typename P2, typename P3,
            typename P4>
  R DoCall(F* f, P1 p1, P2 p2, P3 p3, P4 p4) {
    return f(p1, p2, p3, p4);
  }
#endif

#ifndef DEBUG
  void VerifyParameters0() {}

  template <typename P1>
  void VerifyParameters1() {}

  template <typename P1, typename P2>
  void VerifyParameters2() {}

  template <typename P1, typename P2, typename P3>
  void VerifyParameters3() {}

  template <typename P1, typename P2, typename P3, typename P4>
  void VerifyParameters4() {}
#else
  void VerifyParameters0() { VerifyParameters(0, NULL); }

  template <typename P1>
  void VerifyParameters1() {
    MachineType parameters[] = {ReturnValueTraits<P1>::Representation()};
    VerifyParameters(arraysize(parameters), parameters);
  }

  template <typename P1, typename P2>
  void VerifyParameters2() {
    MachineType parameters[] = {ReturnValueTraits<P1>::Representation(),
                                ReturnValueTraits<P2>::Representation()};
    VerifyParameters(arraysize(parameters), parameters);
  }

  template <typename P1, typename P2, typename P3>
  void VerifyParameters3() {
    MachineType parameters[] = {ReturnValueTraits<P1>::Representation(),
                                ReturnValueTraits<P2>::Representation(),
                                ReturnValueTraits<P3>::Representation()};
    VerifyParameters(arraysize(parameters), parameters);
  }

  template <typename P1, typename P2, typename P3, typename P4>
  void VerifyParameters4() {
    MachineType parameters[] = {ReturnValueTraits<P1>::Representation(),
                                ReturnValueTraits<P2>::Representation(),
                                ReturnValueTraits<P3>::Representation(),
                                ReturnValueTraits<P4>::Representation()};
    VerifyParameters(arraysize(parameters), parameters);
  }
#endif

  // TODO(dcarney): replace Call() in CallHelper2 with these.
  template <typename R>
  R Call0() {
    typedef R V8_CDECL FType();
    VerifyParameters0();
    return DoCall<R>(FUNCTION_CAST<FType*>(Generate()));
  }

  template <typename R, typename P1>
  R Call1(P1 p1) {
    typedef R V8_CDECL FType(P1);
    VerifyParameters1<P1>();
    return DoCall<R>(FUNCTION_CAST<FType*>(Generate()), p1);
  }

  template <typename R, typename P1, typename P2>
  R Call2(P1 p1, P2 p2) {
    typedef R V8_CDECL FType(P1, P2);
    VerifyParameters2<P1, P2>();
    return DoCall<R>(FUNCTION_CAST<FType*>(Generate()), p1, p2);
  }

  template <typename R, typename P1, typename P2, typename P3>
  R Call3(P1 p1, P2 p2, P3 p3) {
    typedef R V8_CDECL FType(P1, P2, P3);
    VerifyParameters3<P1, P2, P3>();
    return DoCall<R>(FUNCTION_CAST<FType*>(Generate()), p1, p2, p3);
  }

  template <typename R, typename P1, typename P2, typename P3, typename P4>
  R Call4(P1 p1, P2 p2, P3 p3, P4 p4) {
    typedef R V8_CDECL FType(P1, P2, P3, P4);
    VerifyParameters4<P1, P2, P3, P4>();
    return DoCall<R>(FUNCTION_CAST<FType*>(Generate()), p1, p2, p3, p4);
  }

  template <typename R, typename C>
  friend class CallHelper2;
  Isolate* isolate_;
};


// TODO(dcarney): replace CallHelper with CallHelper2 and rename.
template <typename R, typename C>
class CallHelper2 {
 public:
  R Call() { return helper()->template Call0<R>(); }

  template <typename P1>
  R Call(P1 p1) {
    return helper()->template Call1<R>(p1);
  }

  template <typename P1, typename P2>
  R Call(P1 p1, P2 p2) {
    return helper()->template Call2<R>(p1, p2);
  }

  template <typename P1, typename P2, typename P3>
  R Call(P1 p1, P2 p2, P3 p3) {
    return helper()->template Call3<R>(p1, p2, p3);
  }

  template <typename P1, typename P2, typename P3, typename P4>
  R Call(P1 p1, P2 p2, P3 p3, P4 p4) {
    return helper()->template Call4<R>(p1, p2, p3, p4);
  }

 private:
  CallHelper* helper() { return static_cast<C*>(this); }
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_CCTEST_COMPILER_CALL_TESTER_H_
