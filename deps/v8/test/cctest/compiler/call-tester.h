// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CCTEST_COMPILER_CALL_TESTER_H_
#define V8_CCTEST_COMPILER_CALL_TESTER_H_

#include "src/simulator.h"
#include "test/cctest/compiler/c-signature.h"

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

template <typename R>
inline R CastReturnValue(uintptr_t r) {
  return reinterpret_cast<R>(r);
}

template <>
inline void CastReturnValue(uintptr_t r) {}

template <>
inline bool CastReturnValue(uintptr_t r) {
  return static_cast<bool>(r);
}

template <>
inline int32_t CastReturnValue(uintptr_t r) {
  return static_cast<int32_t>(r);
}

template <>
inline uint32_t CastReturnValue(uintptr_t r) {
  return static_cast<uint32_t>(r);
}

template <>
inline int64_t CastReturnValue(uintptr_t r) {
  return static_cast<int64_t>(r);
}

template <>
inline uint64_t CastReturnValue(uintptr_t r) {
  return static_cast<uint64_t>(r);
}

template <>
inline int16_t CastReturnValue(uintptr_t r) {
  return static_cast<int16_t>(r);
}

template <>
inline uint16_t CastReturnValue(uintptr_t r) {
  return static_cast<uint16_t>(r);
}

template <>
inline int8_t CastReturnValue(uintptr_t r) {
  return static_cast<int8_t>(r);
}

template <>
inline uint8_t CastReturnValue(uintptr_t r) {
  return static_cast<uint8_t>(r);
}

template <>
inline double CastReturnValue(uintptr_t r) {
  UNREACHABLE();
  return 0.0;
}

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


#if !V8_TARGET_ARCH_32_BIT

// Additional template specialization required for mips64 to sign-extend
// parameters defined by calling convention.
template <>
struct ParameterTraits<int32_t> {
  static int64_t Cast(int32_t r) { return static_cast<int64_t>(r); }
};

#if !V8_TARGET_ARCH_PPC64
template <>
struct ParameterTraits<uint32_t> {
  static int64_t Cast(uint32_t r) {
    return static_cast<int64_t>(static_cast<int32_t>(r));
  }
};
#endif

#endif  // !V8_TARGET_ARCH_64_BIT


template <typename R>
class CallHelper {
 public:
  explicit CallHelper(Isolate* isolate, MachineSignature* csig)
      : csig_(csig), isolate_(isolate) {
    USE(isolate_);
  }
  virtual ~CallHelper() {}

  R Call() {
    typedef R V8_CDECL FType();
    CSignature::VerifyParams(csig_);
    return DoCall(FUNCTION_CAST<FType*>(Generate()));
  }

  template <typename P1>
  R Call(P1 p1) {
    typedef R V8_CDECL FType(P1);
    CSignature::VerifyParams<P1>(csig_);
    return DoCall(FUNCTION_CAST<FType*>(Generate()), p1);
  }

  template <typename P1, typename P2>
  R Call(P1 p1, P2 p2) {
    typedef R V8_CDECL FType(P1, P2);
    CSignature::VerifyParams<P1, P2>(csig_);
    return DoCall(FUNCTION_CAST<FType*>(Generate()), p1, p2);
  }

  template <typename P1, typename P2, typename P3>
  R Call(P1 p1, P2 p2, P3 p3) {
    typedef R V8_CDECL FType(P1, P2, P3);
    CSignature::VerifyParams<P1, P2, P3>(csig_);
    return DoCall(FUNCTION_CAST<FType*>(Generate()), p1, p2, p3);
  }

  template <typename P1, typename P2, typename P3, typename P4>
  R Call(P1 p1, P2 p2, P3 p3, P4 p4) {
    typedef R V8_CDECL FType(P1, P2, P3, P4);
    CSignature::VerifyParams<P1, P2, P3, P4>(csig_);
    return DoCall(FUNCTION_CAST<FType*>(Generate()), p1, p2, p3, p4);
  }

  template <typename P1, typename P2, typename P3, typename P4, typename P5>
  R Call(P1 p1, P2 p2, P3 p3, P4 p4, P5 p5) {
    typedef R V8_CDECL FType(P1, P2, P3, P4, P5);
    CSignature::VerifyParams<P1, P2, P3, P4, P5>(csig_);
    return DoCall(FUNCTION_CAST<FType*>(Generate()), p1, p2, p3, p4, p5);
  }

 protected:
  MachineSignature* csig_;

  virtual byte* Generate() = 0;

 private:
#if USE_SIMULATOR && V8_TARGET_ARCH_ARM64
  uintptr_t CallSimulator(byte* f, Simulator::CallArgument* args) {
    Simulator* simulator = Simulator::current(isolate_);
    return static_cast<uintptr_t>(simulator->CallInt64(f, args));
  }

  template <typename F>
  R DoCall(F* f) {
    Simulator::CallArgument args[] = {Simulator::CallArgument::End()};
    return CastReturnValue<R>(CallSimulator(FUNCTION_ADDR(f), args));
  }
  template <typename F, typename P1>
  R DoCall(F* f, P1 p1) {
    Simulator::CallArgument args[] = {Simulator::CallArgument(p1),
                                      Simulator::CallArgument::End()};
    return CastReturnValue<R>(CallSimulator(FUNCTION_ADDR(f), args));
  }
  template <typename F, typename P1, typename P2>
  R DoCall(F* f, P1 p1, P2 p2) {
    Simulator::CallArgument args[] = {Simulator::CallArgument(p1),
                                      Simulator::CallArgument(p2),
                                      Simulator::CallArgument::End()};
    return CastReturnValue<R>(CallSimulator(FUNCTION_ADDR(f), args));
  }
  template <typename F, typename P1, typename P2, typename P3>
  R DoCall(F* f, P1 p1, P2 p2, P3 p3) {
    Simulator::CallArgument args[] = {
        Simulator::CallArgument(p1), Simulator::CallArgument(p2),
        Simulator::CallArgument(p3), Simulator::CallArgument::End()};
    return CastReturnValue<R>(CallSimulator(FUNCTION_ADDR(f), args));
  }
  template <typename F, typename P1, typename P2, typename P3, typename P4>
  R DoCall(F* f, P1 p1, P2 p2, P3 p3, P4 p4) {
    Simulator::CallArgument args[] = {
        Simulator::CallArgument(p1), Simulator::CallArgument(p2),
        Simulator::CallArgument(p3), Simulator::CallArgument(p4),
        Simulator::CallArgument::End()};
    return CastReturnValue<R>(CallSimulator(FUNCTION_ADDR(f), args));
  }
  template <typename F, typename P1, typename P2, typename P3, typename P4,
            typename P5>
  R DoCall(F* f, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5) {
    Simulator::CallArgument args[] = {
        Simulator::CallArgument(p1), Simulator::CallArgument(p2),
        Simulator::CallArgument(p3), Simulator::CallArgument(p4),
        Simulator::CallArgument(p5), Simulator::CallArgument::End()};
    return CastReturnValue<R>(CallSimulator(FUNCTION_ADDR(f), args));
  }
#elif USE_SIMULATOR && \
    (V8_TARGET_ARCH_MIPS64 || V8_TARGET_ARCH_PPC64 || V8_TARGET_ARCH_S390X)
  uintptr_t CallSimulator(byte* f, int64_t p1 = 0, int64_t p2 = 0,
                          int64_t p3 = 0, int64_t p4 = 0, int64_t p5 = 0) {
    Simulator* simulator = Simulator::current(isolate_);
    return static_cast<uintptr_t>(simulator->Call(f, 5, p1, p2, p3, p4, p5));
  }


  template <typename F>
  R DoCall(F* f) {
    return CastReturnValue<R>(CallSimulator(FUNCTION_ADDR(f)));
  }
  template <typename F, typename P1>
  R DoCall(F* f, P1 p1) {
    return CastReturnValue<R>(
        CallSimulator(FUNCTION_ADDR(f), ParameterTraits<P1>::Cast(p1)));
  }
  template <typename F, typename P1, typename P2>
  R DoCall(F* f, P1 p1, P2 p2) {
    return CastReturnValue<R>(CallSimulator(FUNCTION_ADDR(f),
                                            ParameterTraits<P1>::Cast(p1),
                                            ParameterTraits<P2>::Cast(p2)));
  }
  template <typename F, typename P1, typename P2, typename P3>
  R DoCall(F* f, P1 p1, P2 p2, P3 p3) {
    return CastReturnValue<R>(CallSimulator(
        FUNCTION_ADDR(f), ParameterTraits<P1>::Cast(p1),
        ParameterTraits<P2>::Cast(p2), ParameterTraits<P3>::Cast(p3)));
  }
  template <typename F, typename P1, typename P2, typename P3, typename P4>
  R DoCall(F* f, P1 p1, P2 p2, P3 p3, P4 p4) {
    return CastReturnValue<R>(CallSimulator(
        FUNCTION_ADDR(f), ParameterTraits<P1>::Cast(p1),
        ParameterTraits<P2>::Cast(p2), ParameterTraits<P3>::Cast(p3),
        ParameterTraits<P4>::Cast(p4)));
  }
  template <typename F, typename P1, typename P2, typename P3, typename P4,
            typename P5>
  R DoCall(F* f, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5) {
    return CastReturnValue<R>(CallSimulator(
        FUNCTION_ADDR(f), ParameterTraits<P1>::Cast(p1),
        ParameterTraits<P2>::Cast(p2), ParameterTraits<P3>::Cast(p3),
        ParameterTraits<P4>::Cast(p4), ParameterTraits<P5>::Cast(p5)));
  }
#elif USE_SIMULATOR && (V8_TARGET_ARCH_ARM || V8_TARGET_ARCH_MIPS || \
                        V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_S390)
  uintptr_t CallSimulator(byte* f, int32_t p1 = 0, int32_t p2 = 0,
                          int32_t p3 = 0, int32_t p4 = 0, int32_t p5 = 0) {
    Simulator* simulator = Simulator::current(isolate_);
    return static_cast<uintptr_t>(simulator->Call(f, 5, p1, p2, p3, p4, p5));
  }
  template <typename F>
  R DoCall(F* f) {
    return CastReturnValue<R>(CallSimulator(FUNCTION_ADDR(f)));
  }
  template <typename F, typename P1>
  R DoCall(F* f, P1 p1) {
    return CastReturnValue<R>(
        CallSimulator(FUNCTION_ADDR(f), ParameterTraits<P1>::Cast(p1)));
  }
  template <typename F, typename P1, typename P2>
  R DoCall(F* f, P1 p1, P2 p2) {
    return CastReturnValue<R>(CallSimulator(FUNCTION_ADDR(f),
                                            ParameterTraits<P1>::Cast(p1),
                                            ParameterTraits<P2>::Cast(p2)));
  }
  template <typename F, typename P1, typename P2, typename P3>
  R DoCall(F* f, P1 p1, P2 p2, P3 p3) {
    return CastReturnValue<R>(CallSimulator(
        FUNCTION_ADDR(f), ParameterTraits<P1>::Cast(p1),
        ParameterTraits<P2>::Cast(p2), ParameterTraits<P3>::Cast(p3)));
  }
  template <typename F, typename P1, typename P2, typename P3, typename P4>
  R DoCall(F* f, P1 p1, P2 p2, P3 p3, P4 p4) {
    return CastReturnValue<R>(CallSimulator(
        FUNCTION_ADDR(f), ParameterTraits<P1>::Cast(p1),
        ParameterTraits<P2>::Cast(p2), ParameterTraits<P3>::Cast(p3),
        ParameterTraits<P4>::Cast(p4)));
  }
  template <typename F, typename P1, typename P2, typename P3, typename P4,
            typename P5>
  R DoCall(F* f, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5) {
    return CastReturnValue<R>(CallSimulator(
        FUNCTION_ADDR(f), ParameterTraits<P1>::Cast(p1),
        ParameterTraits<P2>::Cast(p2), ParameterTraits<P3>::Cast(p3),
        ParameterTraits<P4>::Cast(p4), ParameterTraits<P5>::Cast(p5)));
  }
#else
  template <typename F>
  R DoCall(F* f) {
    return f();
  }
  template <typename F, typename P1>
  R DoCall(F* f, P1 p1) {
    return f(p1);
  }
  template <typename F, typename P1, typename P2>
  R DoCall(F* f, P1 p1, P2 p2) {
    return f(p1, p2);
  }
  template <typename F, typename P1, typename P2, typename P3>
  R DoCall(F* f, P1 p1, P2 p2, P3 p3) {
    return f(p1, p2, p3);
  }
  template <typename F, typename P1, typename P2, typename P3, typename P4>
  R DoCall(F* f, P1 p1, P2 p2, P3 p3, P4 p4) {
    return f(p1, p2, p3, p4);
  }
  template <typename F, typename P1, typename P2, typename P3, typename P4,
            typename P5>
  R DoCall(F* f, P1 p1, P2 p2, P3 p3, P4 p4, P5 p5) {
    return f(p1, p2, p3, p4, p5);
  }
#endif

  Isolate* isolate_;
};

// A call helper that calls the given code object assuming C calling convention.
template <typename T>
class CodeRunner : public CallHelper<T> {
 public:
  CodeRunner(Isolate* isolate, Handle<Code> code, MachineSignature* csig)
      : CallHelper<T>(isolate, csig), code_(code) {}
  virtual ~CodeRunner() {}

  virtual byte* Generate() { return code_->entry(); }

 private:
  Handle<Code> code_;
};


}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_CCTEST_COMPILER_CALL_TESTER_H_
