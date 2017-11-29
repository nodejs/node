// Copyright 2016 the V8 project authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include <cmath>
#include <functional>
#include <limits>

#include "src/base/bits.h"
#include "src/base/utils/random-number-generator.h"
#include "src/codegen.h"
#include "src/objects-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/codegen-tester.h"
#include "test/cctest/compiler/graph-builder-tester.h"
#include "test/cctest/compiler/value-helper.h"


namespace {
template <typename Type>
void CheckOobValue(Type val) {
  UNREACHABLE();
}

template <>
void CheckOobValue(int32_t val) {
  CHECK_EQ(0, val);
}

template <>
void CheckOobValue(int64_t val) {
  CHECK_EQ(0, val);
}

template <>
void CheckOobValue(float val) {
  CHECK(std::isnan(val));
}

template <>
void CheckOobValue(double val) {
  CHECK(std::isnan(val));
}
}  // namespace

namespace v8 {
namespace internal {
namespace compiler {

enum TestAlignment {
  kAligned,
  kUnaligned,
};

// This is a America!
#define A_BILLION 1000000000ULL
#define A_GIG (1024ULL * 1024ULL * 1024ULL)

namespace {
void RunLoadInt32(const TestAlignment t) {
  RawMachineAssemblerTester<int32_t> m;

  int32_t p1 = 0;  // loads directly from this location.

  if (t == TestAlignment::kAligned) {
    m.Return(m.LoadFromPointer(&p1, MachineType::Int32()));
  } else if (t == TestAlignment::kUnaligned) {
    m.Return(m.UnalignedLoadFromPointer(&p1, MachineType::Int32()));
  } else {
    UNREACHABLE();
  }

  FOR_INT32_INPUTS(i) {
    p1 = *i;
    CHECK_EQ(p1, m.Call());
  }
}

void RunLoadInt32Offset(TestAlignment t) {
  int32_t p1 = 0;  // loads directly from this location.

  int32_t offsets[] = {-2000000, -100, -101, 1,          3,
                       7,        120,  2000, 2000000000, 0xff};

  for (size_t i = 0; i < arraysize(offsets); i++) {
    RawMachineAssemblerTester<int32_t> m;
    int32_t offset = offsets[i];
    byte* pointer = reinterpret_cast<byte*>(&p1) - offset;

    // generate load [#base + #index]
    if (t == TestAlignment::kAligned) {
      m.Return(m.LoadFromPointer(pointer, MachineType::Int32(), offset));
    } else if (t == TestAlignment::kUnaligned) {
      m.Return(
          m.UnalignedLoadFromPointer(pointer, MachineType::Int32(), offset));
    } else {
      UNREACHABLE();
    }

    FOR_INT32_INPUTS(j) {
      p1 = *j;
      CHECK_EQ(p1, m.Call());
    }
  }
}

void RunLoadStoreFloat32Offset(TestAlignment t) {
  float p1 = 0.0f;  // loads directly from this location.
  float p2 = 0.0f;  // and stores directly into this location.

  FOR_INT32_INPUTS(i) {
    int32_t magic = 0x2342aabb + *i * 3;
    RawMachineAssemblerTester<int32_t> m;
    int32_t offset = *i;
    byte* from = reinterpret_cast<byte*>(&p1) - offset;
    byte* to = reinterpret_cast<byte*>(&p2) - offset;
    // generate load [#base + #index]
    if (t == TestAlignment::kAligned) {
      Node* load = m.Load(MachineType::Float32(), m.PointerConstant(from),
                          m.IntPtrConstant(offset));
      m.Store(MachineRepresentation::kFloat32, m.PointerConstant(to),
              m.IntPtrConstant(offset), load, kNoWriteBarrier);
    } else if (t == TestAlignment::kUnaligned) {
      Node* load =
          m.UnalignedLoad(MachineType::Float32(), m.PointerConstant(from),
                          m.IntPtrConstant(offset));
      m.UnalignedStore(MachineRepresentation::kFloat32, m.PointerConstant(to),
                       m.IntPtrConstant(offset), load);

    } else {
      UNREACHABLE();
    }
    m.Return(m.Int32Constant(magic));

    FOR_FLOAT32_INPUTS(j) {
      p1 = *j;
      p2 = *j - 5;
      CHECK_EQ(magic, m.Call());
      CheckDoubleEq(p1, p2);
    }
  }
}

void RunLoadStoreFloat64Offset(TestAlignment t) {
  double p1 = 0;  // loads directly from this location.
  double p2 = 0;  // and stores directly into this location.

  FOR_INT32_INPUTS(i) {
    int32_t magic = 0x2342aabb + *i * 3;
    RawMachineAssemblerTester<int32_t> m;
    int32_t offset = *i;
    byte* from = reinterpret_cast<byte*>(&p1) - offset;
    byte* to = reinterpret_cast<byte*>(&p2) - offset;
    // generate load [#base + #index]
    if (t == TestAlignment::kAligned) {
      Node* load = m.Load(MachineType::Float64(), m.PointerConstant(from),
                          m.IntPtrConstant(offset));
      m.Store(MachineRepresentation::kFloat64, m.PointerConstant(to),
              m.IntPtrConstant(offset), load, kNoWriteBarrier);
    } else if (t == TestAlignment::kUnaligned) {
      Node* load =
          m.UnalignedLoad(MachineType::Float64(), m.PointerConstant(from),
                          m.IntPtrConstant(offset));
      m.UnalignedStore(MachineRepresentation::kFloat64, m.PointerConstant(to),
                       m.IntPtrConstant(offset), load);
    } else {
      UNREACHABLE();
    }
    m.Return(m.Int32Constant(magic));

    FOR_FLOAT64_INPUTS(j) {
      p1 = *j;
      p2 = *j - 5;
      CHECK_EQ(magic, m.Call());
      CheckDoubleEq(p1, p2);
    }
  }
}
}  // namespace

TEST(RunLoadInt32) { RunLoadInt32(TestAlignment::kAligned); }

TEST(RunUnalignedLoadInt32) { RunLoadInt32(TestAlignment::kUnaligned); }

TEST(RunLoadInt32Offset) { RunLoadInt32Offset(TestAlignment::kAligned); }

TEST(RunUnalignedLoadInt32Offset) {
  RunLoadInt32Offset(TestAlignment::kUnaligned);
}

TEST(RunLoadStoreFloat32Offset) {
  RunLoadStoreFloat32Offset(TestAlignment::kAligned);
}

TEST(RunUnalignedLoadStoreFloat32Offset) {
  RunLoadStoreFloat32Offset(TestAlignment::kUnaligned);
}

TEST(RunLoadStoreFloat64Offset) {
  RunLoadStoreFloat64Offset(TestAlignment::kAligned);
}

TEST(RunUnalignedLoadStoreFloat64Offset) {
  RunLoadStoreFloat64Offset(TestAlignment::kUnaligned);
}

namespace {
template <typename Type>
void RunLoadImmIndex(MachineType rep, TestAlignment t) {
  const int kNumElems = 3;
  Type buffer[kNumElems];

  // initialize the buffer with some raw data.
  byte* raw = reinterpret_cast<byte*>(buffer);
  for (size_t i = 0; i < sizeof(buffer); i++) {
    raw[i] = static_cast<byte>((i + sizeof(buffer)) ^ 0xAA);
  }

  // Test with various large and small offsets.
  for (int offset = -1; offset <= 200000; offset *= -5) {
    for (int i = 0; i < kNumElems; i++) {
      BufferedRawMachineAssemblerTester<Type> m;
      Node* base = m.PointerConstant(buffer - offset);
      Node* index = m.Int32Constant((offset + i) * sizeof(buffer[0]));
      if (t == TestAlignment::kAligned) {
        m.Return(m.Load(rep, base, index));
      } else if (t == TestAlignment::kUnaligned) {
        m.Return(m.UnalignedLoad(rep, base, index));
      } else {
        UNREACHABLE();
      }

      volatile Type expected = buffer[i];
      volatile Type actual = m.Call();
      CHECK_EQ(expected, actual);
    }
  }
}

template <typename CType>
void RunLoadStore(MachineType rep, TestAlignment t) {
  const int kNumElems = 4;
  CType buffer[kNumElems];

  for (int32_t x = 0; x < kNumElems; x++) {
    int32_t y = kNumElems - x - 1;
    // initialize the buffer with raw data.
    byte* raw = reinterpret_cast<byte*>(buffer);
    for (size_t i = 0; i < sizeof(buffer); i++) {
      raw[i] = static_cast<byte>((i + sizeof(buffer)) ^ 0xAA);
    }

    RawMachineAssemblerTester<int32_t> m;
    int32_t OK = 0x29000 + x;
    Node* base = m.PointerConstant(buffer);
    Node* index0 = m.IntPtrConstant(x * sizeof(buffer[0]));
    Node* index1 = m.IntPtrConstant(y * sizeof(buffer[0]));
    if (t == TestAlignment::kAligned) {
      Node* load = m.Load(rep, base, index0);
      m.Store(rep.representation(), base, index1, load, kNoWriteBarrier);
    } else if (t == TestAlignment::kUnaligned) {
      Node* load = m.UnalignedLoad(rep, base, index0);
      m.UnalignedStore(rep.representation(), base, index1, load);
    }

    m.Return(m.Int32Constant(OK));

    CHECK(buffer[x] != buffer[y]);
    CHECK_EQ(OK, m.Call());
    CHECK(buffer[x] == buffer[y]);
  }
}

template <typename CType>
void RunUnalignedLoadStoreUnalignedAccess(MachineType rep) {
  CType in, out;
  CType in_buffer[2];
  CType out_buffer[2];
  byte* raw;

  for (int x = 0; x < static_cast<int>(sizeof(CType)); x++) {
    int y = sizeof(CType) - x;

    raw = reinterpret_cast<byte*>(&in);
    for (size_t i = 0; i < sizeof(CType); i++) {
      raw[i] = static_cast<byte>((i + sizeof(CType)) ^ 0xAA);
    }

    raw = reinterpret_cast<byte*>(in_buffer);
    MemCopy(raw + x, &in, sizeof(CType));

    RawMachineAssemblerTester<int32_t> m;
    int32_t OK = 0x29000 + x;

    Node* base0 = m.PointerConstant(in_buffer);
    Node* base1 = m.PointerConstant(out_buffer);
    Node* index0 = m.IntPtrConstant(x);
    Node* index1 = m.IntPtrConstant(y);
    Node* load = m.UnalignedLoad(rep, base0, index0);
    m.UnalignedStore(rep.representation(), base1, index1, load);

    m.Return(m.Int32Constant(OK));

    CHECK_EQ(OK, m.Call());

    raw = reinterpret_cast<byte*>(&out_buffer);
    MemCopy(&out, raw + y, sizeof(CType));
    CHECK(in == out);
  }
}
}  // namespace

TEST(RunLoadImmIndex) {
  RunLoadImmIndex<int8_t>(MachineType::Int8(), TestAlignment::kAligned);
  RunLoadImmIndex<uint8_t>(MachineType::Uint8(), TestAlignment::kAligned);
  RunLoadImmIndex<int16_t>(MachineType::Int16(), TestAlignment::kAligned);
  RunLoadImmIndex<uint16_t>(MachineType::Uint16(), TestAlignment::kAligned);
  RunLoadImmIndex<int32_t>(MachineType::Int32(), TestAlignment::kAligned);
  RunLoadImmIndex<uint32_t>(MachineType::Uint32(), TestAlignment::kAligned);
  RunLoadImmIndex<int32_t*>(MachineType::AnyTagged(), TestAlignment::kAligned);
  RunLoadImmIndex<float>(MachineType::Float32(), TestAlignment::kAligned);
  RunLoadImmIndex<double>(MachineType::Float64(), TestAlignment::kAligned);
#if V8_TARGET_ARCH_64_BIT
  RunLoadImmIndex<int64_t>(MachineType::Int64(), TestAlignment::kAligned);
#endif
  // TODO(titzer): test various indexing modes.
}

TEST(RunUnalignedLoadImmIndex) {
  RunLoadImmIndex<int16_t>(MachineType::Int16(), TestAlignment::kUnaligned);
  RunLoadImmIndex<uint16_t>(MachineType::Uint16(), TestAlignment::kUnaligned);
  RunLoadImmIndex<int32_t>(MachineType::Int32(), TestAlignment::kUnaligned);
  RunLoadImmIndex<uint32_t>(MachineType::Uint32(), TestAlignment::kUnaligned);
  RunLoadImmIndex<int32_t*>(MachineType::AnyTagged(),
                            TestAlignment::kUnaligned);
  RunLoadImmIndex<float>(MachineType::Float32(), TestAlignment::kUnaligned);
  RunLoadImmIndex<double>(MachineType::Float64(), TestAlignment::kUnaligned);
#if V8_TARGET_ARCH_64_BIT
  RunLoadImmIndex<int64_t>(MachineType::Int64(), TestAlignment::kUnaligned);
#endif
  // TODO(titzer): test various indexing modes.
}

TEST(RunLoadStore) {
  RunLoadStore<int8_t>(MachineType::Int8(), TestAlignment::kAligned);
  RunLoadStore<uint8_t>(MachineType::Uint8(), TestAlignment::kAligned);
  RunLoadStore<int16_t>(MachineType::Int16(), TestAlignment::kAligned);
  RunLoadStore<uint16_t>(MachineType::Uint16(), TestAlignment::kAligned);
  RunLoadStore<int32_t>(MachineType::Int32(), TestAlignment::kAligned);
  RunLoadStore<uint32_t>(MachineType::Uint32(), TestAlignment::kAligned);
  RunLoadStore<void*>(MachineType::AnyTagged(), TestAlignment::kAligned);
  RunLoadStore<float>(MachineType::Float32(), TestAlignment::kAligned);
  RunLoadStore<double>(MachineType::Float64(), TestAlignment::kAligned);
#if V8_TARGET_ARCH_64_BIT
  RunLoadStore<int64_t>(MachineType::Int64(), TestAlignment::kAligned);
#endif
}

TEST(RunUnalignedLoadStore) {
  RunLoadStore<int16_t>(MachineType::Int16(), TestAlignment::kUnaligned);
  RunLoadStore<uint16_t>(MachineType::Uint16(), TestAlignment::kUnaligned);
  RunLoadStore<int32_t>(MachineType::Int32(), TestAlignment::kUnaligned);
  RunLoadStore<uint32_t>(MachineType::Uint32(), TestAlignment::kUnaligned);
  RunLoadStore<void*>(MachineType::AnyTagged(), TestAlignment::kUnaligned);
  RunLoadStore<float>(MachineType::Float32(), TestAlignment::kUnaligned);
  RunLoadStore<double>(MachineType::Float64(), TestAlignment::kUnaligned);
#if V8_TARGET_ARCH_64_BIT
  RunLoadStore<int64_t>(MachineType::Int64(), TestAlignment::kUnaligned);
#endif
}

TEST(RunUnalignedLoadStoreUnalignedAccess) {
  RunUnalignedLoadStoreUnalignedAccess<int16_t>(MachineType::Int16());
  RunUnalignedLoadStoreUnalignedAccess<uint16_t>(MachineType::Uint16());
  RunUnalignedLoadStoreUnalignedAccess<int32_t>(MachineType::Int32());
  RunUnalignedLoadStoreUnalignedAccess<uint32_t>(MachineType::Uint32());
  RunUnalignedLoadStoreUnalignedAccess<void*>(MachineType::AnyTagged());
  RunUnalignedLoadStoreUnalignedAccess<float>(MachineType::Float32());
  RunUnalignedLoadStoreUnalignedAccess<double>(MachineType::Float64());
#if V8_TARGET_ARCH_64_BIT
  RunUnalignedLoadStoreUnalignedAccess<int64_t>(MachineType::Int64());
#endif
}

#if V8_TARGET_LITTLE_ENDIAN
#define LSB(addr, bytes) addr
#elif V8_TARGET_BIG_ENDIAN
#define LSB(addr, bytes) reinterpret_cast<byte*>(addr + 1) - bytes
#else
#error "Unknown Architecture"
#endif

namespace {
void RunLoadStoreSignExtend32(TestAlignment t) {
  int32_t buffer[4];
  RawMachineAssemblerTester<int32_t> m;
  Node* load8 = m.LoadFromPointer(LSB(&buffer[0], 1), MachineType::Int8());
  if (t == TestAlignment::kAligned) {
    Node* load16 = m.LoadFromPointer(LSB(&buffer[0], 2), MachineType::Int16());
    Node* load32 = m.LoadFromPointer(&buffer[0], MachineType::Int32());
    m.StoreToPointer(&buffer[1], MachineRepresentation::kWord32, load8);
    m.StoreToPointer(&buffer[2], MachineRepresentation::kWord32, load16);
    m.StoreToPointer(&buffer[3], MachineRepresentation::kWord32, load32);
  } else if (t == TestAlignment::kUnaligned) {
    Node* load16 =
        m.UnalignedLoadFromPointer(LSB(&buffer[0], 2), MachineType::Int16());
    Node* load32 = m.UnalignedLoadFromPointer(&buffer[0], MachineType::Int32());
    m.StoreToPointer(&buffer[1], MachineRepresentation::kWord32, load8);
    m.UnalignedStoreToPointer(&buffer[2], MachineRepresentation::kWord32,
                              load16);
    m.UnalignedStoreToPointer(&buffer[3], MachineRepresentation::kWord32,
                              load32);
  } else {
    UNREACHABLE();
  }
  m.Return(load8);

  FOR_INT32_INPUTS(i) {
    buffer[0] = *i;

    CHECK_EQ(static_cast<int8_t>(*i & 0xff), m.Call());
    CHECK_EQ(static_cast<int8_t>(*i & 0xff), buffer[1]);
    CHECK_EQ(static_cast<int16_t>(*i & 0xffff), buffer[2]);
    CHECK_EQ(*i, buffer[3]);
  }
}

void RunLoadStoreZeroExtend32(TestAlignment t) {
  uint32_t buffer[4];
  RawMachineAssemblerTester<uint32_t> m;
  Node* load8 = m.LoadFromPointer(LSB(&buffer[0], 1), MachineType::Uint8());
  if (t == TestAlignment::kAligned) {
    Node* load16 = m.LoadFromPointer(LSB(&buffer[0], 2), MachineType::Uint16());
    Node* load32 = m.LoadFromPointer(&buffer[0], MachineType::Uint32());
    m.StoreToPointer(&buffer[1], MachineRepresentation::kWord32, load8);
    m.StoreToPointer(&buffer[2], MachineRepresentation::kWord32, load16);
    m.StoreToPointer(&buffer[3], MachineRepresentation::kWord32, load32);
  } else if (t == TestAlignment::kUnaligned) {
    Node* load16 =
        m.UnalignedLoadFromPointer(LSB(&buffer[0], 2), MachineType::Uint16());
    Node* load32 =
        m.UnalignedLoadFromPointer(&buffer[0], MachineType::Uint32());
    m.StoreToPointer(&buffer[1], MachineRepresentation::kWord32, load8);
    m.UnalignedStoreToPointer(&buffer[2], MachineRepresentation::kWord32,
                              load16);
    m.UnalignedStoreToPointer(&buffer[3], MachineRepresentation::kWord32,
                              load32);
  }
  m.Return(load8);

  FOR_UINT32_INPUTS(i) {
    buffer[0] = *i;

    CHECK_EQ((*i & 0xff), m.Call());
    CHECK_EQ((*i & 0xff), buffer[1]);
    CHECK_EQ((*i & 0xffff), buffer[2]);
    CHECK_EQ(*i, buffer[3]);
  }
}
}  // namespace

TEST(RunLoadStoreSignExtend32) {
  RunLoadStoreSignExtend32(TestAlignment::kAligned);
}

TEST(RunUnalignedLoadStoreSignExtend32) {
  RunLoadStoreSignExtend32(TestAlignment::kUnaligned);
}

TEST(RunLoadStoreZeroExtend32) {
  RunLoadStoreZeroExtend32(TestAlignment::kAligned);
}

TEST(RunUnalignedLoadStoreZeroExtend32) {
  RunLoadStoreZeroExtend32(TestAlignment::kUnaligned);
}

#if V8_TARGET_ARCH_64_BIT

namespace {
void RunLoadStoreSignExtend64(TestAlignment t) {
  if (true) return;  // TODO(titzer): sign extension of loads to 64-bit.
  int64_t buffer[5];
  RawMachineAssemblerTester<int64_t> m;
  Node* load8 = m.LoadFromPointer(LSB(&buffer[0], 1), MachineType::Int8());
  if (t == TestAlignment::kAligned) {
    Node* load16 = m.LoadFromPointer(LSB(&buffer[0], 2), MachineType::Int16());
    Node* load32 = m.LoadFromPointer(LSB(&buffer[0], 4), MachineType::Int32());
    Node* load64 = m.LoadFromPointer(&buffer[0], MachineType::Int64());
    m.StoreToPointer(&buffer[1], MachineRepresentation::kWord64, load8);
    m.StoreToPointer(&buffer[2], MachineRepresentation::kWord64, load16);
    m.StoreToPointer(&buffer[3], MachineRepresentation::kWord64, load32);
    m.StoreToPointer(&buffer[4], MachineRepresentation::kWord64, load64);
  } else if (t == TestAlignment::kUnaligned) {
    Node* load16 =
        m.UnalignedLoadFromPointer(LSB(&buffer[0], 2), MachineType::Int16());
    Node* load32 =
        m.UnalignedLoadFromPointer(LSB(&buffer[0], 4), MachineType::Int32());
    Node* load64 = m.UnalignedLoadFromPointer(&buffer[0], MachineType::Int64());
    m.StoreToPointer(&buffer[1], MachineRepresentation::kWord64, load8);
    m.UnalignedStoreToPointer(&buffer[2], MachineRepresentation::kWord64,
                              load16);
    m.UnalignedStoreToPointer(&buffer[3], MachineRepresentation::kWord64,
                              load32);
    m.UnalignedStoreToPointer(&buffer[4], MachineRepresentation::kWord64,
                              load64);
  } else {
    UNREACHABLE();
  }
  m.Return(load8);

  FOR_INT64_INPUTS(i) {
    buffer[0] = *i;

    CHECK_EQ(static_cast<int8_t>(*i & 0xff), m.Call());
    CHECK_EQ(static_cast<int8_t>(*i & 0xff), buffer[1]);
    CHECK_EQ(static_cast<int16_t>(*i & 0xffff), buffer[2]);
    CHECK_EQ(static_cast<int32_t>(*i & 0xffffffff), buffer[3]);
    CHECK_EQ(*i, buffer[4]);
  }
}

void RunLoadStoreZeroExtend64(TestAlignment t) {
  if (kPointerSize < 8) return;
  uint64_t buffer[5];
  RawMachineAssemblerTester<uint64_t> m;
  Node* load8 = m.LoadFromPointer(LSB(&buffer[0], 1), MachineType::Uint8());
  if (t == TestAlignment::kAligned) {
    Node* load16 = m.LoadFromPointer(LSB(&buffer[0], 2), MachineType::Uint16());
    Node* load32 = m.LoadFromPointer(LSB(&buffer[0], 4), MachineType::Uint32());
    Node* load64 = m.LoadFromPointer(&buffer[0], MachineType::Uint64());
    m.StoreToPointer(&buffer[1], MachineRepresentation::kWord64, load8);
    m.StoreToPointer(&buffer[2], MachineRepresentation::kWord64, load16);
    m.StoreToPointer(&buffer[3], MachineRepresentation::kWord64, load32);
    m.StoreToPointer(&buffer[4], MachineRepresentation::kWord64, load64);
  } else if (t == TestAlignment::kUnaligned) {
    Node* load16 =
        m.UnalignedLoadFromPointer(LSB(&buffer[0], 2), MachineType::Uint16());
    Node* load32 =
        m.UnalignedLoadFromPointer(LSB(&buffer[0], 4), MachineType::Uint32());
    Node* load64 =
        m.UnalignedLoadFromPointer(&buffer[0], MachineType::Uint64());
    m.StoreToPointer(&buffer[1], MachineRepresentation::kWord64, load8);
    m.UnalignedStoreToPointer(&buffer[2], MachineRepresentation::kWord64,
                              load16);
    m.UnalignedStoreToPointer(&buffer[3], MachineRepresentation::kWord64,
                              load32);
    m.UnalignedStoreToPointer(&buffer[4], MachineRepresentation::kWord64,
                              load64);
  } else {
    UNREACHABLE();
  }
  m.Return(load8);

  FOR_UINT64_INPUTS(i) {
    buffer[0] = *i;

    CHECK_EQ((*i & 0xff), m.Call());
    CHECK_EQ((*i & 0xff), buffer[1]);
    CHECK_EQ((*i & 0xffff), buffer[2]);
    CHECK_EQ((*i & 0xffffffff), buffer[3]);
    CHECK_EQ(*i, buffer[4]);
  }
}

}  // namespace

TEST(RunCheckedLoadInt64) {
  int64_t buffer[] = {0x66bbccddeeff0011LL, 0x1122334455667788LL};
  RawMachineAssemblerTester<int64_t> m(MachineType::Int32());
  Node* base = m.PointerConstant(buffer);
  Node* index = m.Parameter(0);
  Node* length = m.Int32Constant(16);
  Node* load = m.AddNode(m.machine()->CheckedLoad(MachineType::Int64()), base,
                         index, length);
  m.Return(load);

  CHECK_EQ(buffer[0], m.Call(0));
  CHECK_EQ(buffer[1], m.Call(8));
  CheckOobValue(m.Call(16));
}

TEST(RunLoadStoreSignExtend64) {
  RunLoadStoreSignExtend64(TestAlignment::kAligned);
}

TEST(RunUnalignedLoadStoreSignExtend64) {
  RunLoadStoreSignExtend64(TestAlignment::kUnaligned);
}

TEST(RunLoadStoreZeroExtend64) {
  RunLoadStoreZeroExtend64(TestAlignment::kAligned);
}

TEST(RunUnalignedLoadStoreZeroExtend64) {
  RunLoadStoreZeroExtend64(TestAlignment::kUnaligned);
}

TEST(RunCheckedStoreInt64) {
  const int64_t write = 0x5566778899aabbLL;
  const int64_t before = 0x33bbccddeeff0011LL;
  int64_t buffer[] = {before, before};
  RawMachineAssemblerTester<int32_t> m(MachineType::Int32());
  Node* base = m.PointerConstant(buffer);
  Node* index = m.Parameter(0);
  Node* length = m.Int32Constant(16);
  Node* value = m.Int64Constant(write);
  Node* store =
      m.AddNode(m.machine()->CheckedStore(MachineRepresentation::kWord64), base,
                index, length, value);
  USE(store);
  m.Return(m.Int32Constant(11));

  CHECK_EQ(11, m.Call(16));
  CHECK_EQ(before, buffer[0]);
  CHECK_EQ(before, buffer[1]);

  CHECK_EQ(11, m.Call(0));
  CHECK_EQ(write, buffer[0]);
  CHECK_EQ(before, buffer[1]);

  CHECK_EQ(11, m.Call(8));
  CHECK_EQ(write, buffer[0]);
  CHECK_EQ(write, buffer[1]);
}
#endif

namespace {
template <typename IntType>
void LoadStoreTruncation(MachineType kRepresentation, TestAlignment t) {
  IntType input;

  RawMachineAssemblerTester<int32_t> m;
  Node* ap1;
  if (t == TestAlignment::kAligned) {
    Node* a = m.LoadFromPointer(&input, kRepresentation);
    ap1 = m.Int32Add(a, m.Int32Constant(1));
    m.StoreToPointer(&input, kRepresentation.representation(), ap1);
  } else if (t == TestAlignment::kUnaligned) {
    Node* a = m.UnalignedLoadFromPointer(&input, kRepresentation);
    ap1 = m.Int32Add(a, m.Int32Constant(1));
    m.UnalignedStoreToPointer(&input, kRepresentation.representation(), ap1);
  } else {
    UNREACHABLE();
  }
  m.Return(ap1);

  const IntType max = std::numeric_limits<IntType>::max();
  const IntType min = std::numeric_limits<IntType>::min();

  // Test upper bound.
  input = max;
  CHECK_EQ(max + 1, m.Call());
  CHECK_EQ(min, input);

  // Test lower bound.
  input = min;
  CHECK_EQ(static_cast<IntType>(max + 2), m.Call());
  CHECK_EQ(min + 1, input);

  // Test all one byte values that are not one byte bounds.
  for (int i = -127; i < 127; i++) {
    input = i;
    int expected = i >= 0 ? i + 1 : max + (i - min) + 2;
    CHECK_EQ(static_cast<IntType>(expected), m.Call());
    CHECK_EQ(static_cast<IntType>(i + 1), input);
  }
}
}  // namespace

TEST(RunLoadStoreTruncation) {
  LoadStoreTruncation<int8_t>(MachineType::Int8(), TestAlignment::kAligned);
  LoadStoreTruncation<int16_t>(MachineType::Int16(), TestAlignment::kAligned);
}

TEST(RunUnalignedLoadStoreTruncation) {
  LoadStoreTruncation<int16_t>(MachineType::Int16(), TestAlignment::kUnaligned);
}

void TestRunOobCheckedLoad(bool length_is_immediate) {
  USE(CheckOobValue<int32_t>);
  USE(CheckOobValue<int64_t>);
  USE(CheckOobValue<float>);
  USE(CheckOobValue<double>);

  RawMachineAssemblerTester<int32_t> m(MachineType::Int32(),
                                       MachineType::Int32());
  MachineOperatorBuilder machine(m.zone());
  const int32_t kNumElems = 27;
  const int32_t kLength = kNumElems * 4;

  int32_t buffer[kNumElems];
  Node* base = m.PointerConstant(buffer);
  Node* offset = m.Parameter(0);
  Node* len = length_is_immediate ? m.Int32Constant(kLength) : m.Parameter(1);
  Node* node =
      m.AddNode(machine.CheckedLoad(MachineType::Int32()), base, offset, len);
  m.Return(node);

  {
    // randomize memory.
    v8::base::RandomNumberGenerator rng;
    rng.SetSeed(100);
    rng.NextBytes(&buffer[0], sizeof(buffer));
  }

  // in-bounds accesses.
  for (int32_t i = 0; i < kNumElems; i++) {
    int32_t offset = static_cast<int32_t>(i * sizeof(int32_t));
    int32_t expected = buffer[i];
    CHECK_EQ(expected, m.Call(offset, kLength));
  }

  // slightly out-of-bounds accesses.
  for (int32_t i = kLength; i < kNumElems + 30; i++) {
    int32_t offset = static_cast<int32_t>(i * sizeof(int32_t));
    CheckOobValue(m.Call(offset, kLength));
  }

  // way out-of-bounds accesses.
  for (int32_t offset = -2000000000; offset <= 2000000000;
       offset += 100000000) {
    if (offset == 0) continue;
    CheckOobValue(m.Call(offset, kLength));
  }
}

TEST(RunOobCheckedLoad) { TestRunOobCheckedLoad(false); }

TEST(RunOobCheckedLoadImm) { TestRunOobCheckedLoad(true); }

void TestRunOobCheckedStore(bool length_is_immediate) {
  RawMachineAssemblerTester<int32_t> m(MachineType::Int32(),
                                       MachineType::Int32());
  MachineOperatorBuilder machine(m.zone());
  const int32_t kNumElems = 29;
  const int32_t kValue = -78227234;
  const int32_t kLength = kNumElems * 4;

  int32_t buffer[kNumElems + kNumElems];
  Node* base = m.PointerConstant(buffer);
  Node* offset = m.Parameter(0);
  Node* len = length_is_immediate ? m.Int32Constant(kLength) : m.Parameter(1);
  Node* val = m.Int32Constant(kValue);
  m.AddNode(machine.CheckedStore(MachineRepresentation::kWord32), base, offset,
            len, val);
  m.Return(val);

  // in-bounds accesses.
  for (int32_t i = 0; i < kNumElems; i++) {
    memset(buffer, 0, sizeof(buffer));
    int32_t offset = static_cast<int32_t>(i * sizeof(int32_t));
    CHECK_EQ(kValue, m.Call(offset, kLength));
    for (int32_t j = 0; j < kNumElems + kNumElems; j++) {
      if (i == j) {
        CHECK_EQ(kValue, buffer[j]);
      } else {
        CHECK_EQ(0, buffer[j]);
      }
    }
  }

  memset(buffer, 0, sizeof(buffer));

  // slightly out-of-bounds accesses.
  for (int32_t i = kLength; i < kNumElems + 30; i++) {
    int32_t offset = static_cast<int32_t>(i * sizeof(int32_t));
    CHECK_EQ(kValue, m.Call(offset, kLength));
    for (int32_t j = 0; j < kNumElems + kNumElems; j++) {
      CHECK_EQ(0, buffer[j]);
    }
  }

  // way out-of-bounds accesses.
  for (int32_t offset = -2000000000; offset <= 2000000000;
       offset += 100000000) {
    if (offset == 0) continue;
    CHECK_EQ(kValue, m.Call(offset, kLength));
    for (int32_t j = 0; j < kNumElems + kNumElems; j++) {
      CHECK_EQ(0, buffer[j]);
    }
  }
}

TEST(RunOobCheckedStore) { TestRunOobCheckedStore(false); }

TEST(RunOobCheckedStoreImm) { TestRunOobCheckedStore(true); }

// TODO(titzer): CheckedLoad/CheckedStore don't support 64-bit offsets.
#define ALLOW_64_BIT_OFFSETS 0

#if V8_TARGET_ARCH_64_BIT && ALLOW_64_BIT_OFFSETS

void TestRunOobCheckedLoad64(uint32_t pseudo_base, bool length_is_immediate) {
  RawMachineAssemblerTester<int32_t> m(MachineType::Uint64(),
                                       MachineType::Uint64());
  MachineOperatorBuilder machine(m.zone());
  const uint32_t kNumElems = 25;
  const uint32_t kLength = kNumElems * 4;
  int32_t real_buffer[kNumElems];

  // Simulate the end of a large buffer.
  int32_t* buffer = real_buffer - (pseudo_base / 4);
  uint64_t length = kLength + pseudo_base;

  Node* base = m.PointerConstant(buffer);
  Node* offset = m.Parameter(0);
  Node* len = length_is_immediate ? m.Int64Constant(length) : m.Parameter(1);
  Node* node =
      m.AddNode(machine.CheckedLoad(MachineType::Int32()), base, offset, len);
  m.Return(node);

  {
    // randomize memory.
    v8::base::RandomNumberGenerator rng;
    rng.SetSeed(100);
    rng.NextBytes(&real_buffer[0], sizeof(real_buffer));
  }

  // in-bounds accesses.
  for (uint32_t i = 0; i < kNumElems; i++) {
    uint64_t offset = pseudo_base + i * 4;
    int32_t expected = real_buffer[i];
    CHECK_EQ(expected, m.Call(offset, length));
  }

  // in-bounds accesses w.r.t lower 32-bits, but upper bits set.
  for (uint64_t i = 0x100000000ULL; i != 0; i <<= 1) {
    uint64_t offset = pseudo_base + i;
    CheckOobValue(m.Call(offset, length));
  }

  // slightly out-of-bounds accesses.
  for (uint32_t i = kLength; i < kNumElems + 30; i++) {
    uint64_t offset = pseudo_base + i * 4;
    CheckOobValue(0, m.Call(offset, length));
  }

  // way out-of-bounds accesses.
  for (uint64_t offset = length; offset < 100 * A_BILLION; offset += A_GIG) {
    if (offset < length) continue;
    CheckOobValue(0, m.Call(offset, length));
  }
}

TEST(RunOobCheckedLoad64_0) {
  TestRunOobCheckedLoad64(0, false);
  TestRunOobCheckedLoad64(0, true);
}

TEST(RunOobCheckedLoad64_1) {
  TestRunOobCheckedLoad64(1 * A_BILLION, false);
  TestRunOobCheckedLoad64(1 * A_BILLION, true);
}

TEST(RunOobCheckedLoad64_2) {
  TestRunOobCheckedLoad64(2 * A_BILLION, false);
  TestRunOobCheckedLoad64(2 * A_BILLION, true);
}

TEST(RunOobCheckedLoad64_3) {
  TestRunOobCheckedLoad64(3 * A_BILLION, false);
  TestRunOobCheckedLoad64(3 * A_BILLION, true);
}

TEST(RunOobCheckedLoad64_4) {
  TestRunOobCheckedLoad64(4 * A_BILLION, false);
  TestRunOobCheckedLoad64(4 * A_BILLION, true);
}

void TestRunOobCheckedStore64(uint32_t pseudo_base, bool length_is_immediate) {
  RawMachineAssemblerTester<int32_t> m(MachineType::Uint64(),
                                       MachineType::Uint64());
  MachineOperatorBuilder machine(m.zone());
  const uint32_t kNumElems = 21;
  const uint32_t kLength = kNumElems * 4;
  const uint32_t kValue = 897234987;
  int32_t real_buffer[kNumElems + kNumElems];

  // Simulate the end of a large buffer.
  int32_t* buffer = real_buffer - (pseudo_base / 4);
  uint64_t length = kLength + pseudo_base;

  Node* base = m.PointerConstant(buffer);
  Node* offset = m.Parameter(0);
  Node* len = length_is_immediate ? m.Int64Constant(length) : m.Parameter(1);
  Node* val = m.Int32Constant(kValue);
  m.AddNode(machine.CheckedStore(MachineRepresentation::kWord32), base, offset,
            len, val);
  m.Return(val);

  // in-bounds accesses.
  for (uint32_t i = 0; i < kNumElems; i++) {
    memset(real_buffer, 0, sizeof(real_buffer));
    uint64_t offset = pseudo_base + i * 4;
    CHECK_EQ(kValue, m.Call(offset, length));
    for (uint32_t j = 0; j < kNumElems + kNumElems; j++) {
      if (i == j) {
        CHECK_EQ(kValue, real_buffer[j]);
      } else {
        CHECK_EQ(0, real_buffer[j]);
      }
    }
  }

  memset(real_buffer, 0, sizeof(real_buffer));

  // in-bounds accesses w.r.t lower 32-bits, but upper bits set.
  for (uint64_t i = 0x100000000ULL; i != 0; i <<= 1) {
    uint64_t offset = pseudo_base + i;
    CHECK_EQ(kValue, m.Call(offset, length));
    for (int32_t j = 0; j < kNumElems + kNumElems; j++) {
      CHECK_EQ(0, real_buffer[j]);
    }
  }

  // slightly out-of-bounds accesses.
  for (uint32_t i = kLength; i < kNumElems + 30; i++) {
    uint64_t offset = pseudo_base + i * 4;
    CHECK_EQ(kValue, m.Call(offset, length));
    for (int32_t j = 0; j < kNumElems + kNumElems; j++) {
      CHECK_EQ(0, real_buffer[j]);
    }
  }

  // way out-of-bounds accesses.
  for (uint64_t offset = length; offset < 100 * A_BILLION; offset += A_GIG) {
    if (offset < length) continue;
    CHECK_EQ(kValue, m.Call(offset, length));
    for (int32_t j = 0; j < kNumElems + kNumElems; j++) {
      CHECK_EQ(0, real_buffer[j]);
    }
  }
}

TEST(RunOobCheckedStore64_0) {
  TestRunOobCheckedStore64(0, false);
  TestRunOobCheckedStore64(0, true);
}

TEST(RunOobCheckedStore64_1) {
  TestRunOobCheckedStore64(1 * A_BILLION, false);
  TestRunOobCheckedStore64(1 * A_BILLION, true);
}

TEST(RunOobCheckedStore64_2) {
  TestRunOobCheckedStore64(2 * A_BILLION, false);
  TestRunOobCheckedStore64(2 * A_BILLION, true);
}

TEST(RunOobCheckedStore64_3) {
  TestRunOobCheckedStore64(3 * A_BILLION, false);
  TestRunOobCheckedStore64(3 * A_BILLION, true);
}

TEST(RunOobCheckedStore64_4) {
  TestRunOobCheckedStore64(4 * A_BILLION, false);
  TestRunOobCheckedStore64(4 * A_BILLION, true);
}

#endif

void TestRunOobCheckedLoad_pseudo(uint64_t x, bool length_is_immediate) {
  RawMachineAssemblerTester<int32_t> m(MachineType::Uint32(),
                                       MachineType::Uint32());

  uint32_t pseudo_base = static_cast<uint32_t>(x);
  MachineOperatorBuilder machine(m.zone());
  const uint32_t kNumElems = 29;
  const uint32_t kLength = pseudo_base + kNumElems * 4;

  int32_t buffer[kNumElems];
  Node* base = m.PointerConstant(reinterpret_cast<byte*>(buffer) - pseudo_base);
  Node* offset = m.Parameter(0);
  Node* len = length_is_immediate ? m.Int32Constant(kLength) : m.Parameter(1);
  Node* node =
      m.AddNode(machine.CheckedLoad(MachineType::Int32()), base, offset, len);
  m.Return(node);

  {
    // randomize memory.
    v8::base::RandomNumberGenerator rng;
    rng.SetSeed(100);
    rng.NextBytes(&buffer[0], sizeof(buffer));
  }

  // in-bounds accesses.
  for (uint32_t i = 0; i < kNumElems; i++) {
    uint32_t offset = static_cast<uint32_t>(i * sizeof(int32_t));
    uint32_t expected = buffer[i];
    CHECK_EQ(expected,
             static_cast<uint32_t>(m.Call(offset + pseudo_base, kLength)));
  }

  // slightly out-of-bounds accesses.
  for (uint32_t i = kNumElems; i < kNumElems + 30; i++) {
    uint32_t offset = i * sizeof(int32_t);
    CheckOobValue(m.Call(offset + pseudo_base, kLength));
  }

  // way out-of-bounds accesses.
  for (uint64_t i = pseudo_base + sizeof(buffer); i < 0xFFFFFFFF;
       i += A_BILLION) {
    uint32_t offset = static_cast<uint32_t>(i);
    CheckOobValue(m.Call(offset, kLength));
  }
}

TEST(RunOobCheckedLoad_pseudo0) {
  TestRunOobCheckedLoad_pseudo(0, false);
  TestRunOobCheckedLoad_pseudo(0, true);
}

TEST(RunOobCheckedLoad_pseudo1) {
  TestRunOobCheckedLoad_pseudo(100000, false);
  TestRunOobCheckedLoad_pseudo(100000, true);
}

TEST(RunOobCheckedLoad_pseudo2) {
  TestRunOobCheckedLoad_pseudo(A_BILLION, false);
  TestRunOobCheckedLoad_pseudo(A_BILLION, true);
}

TEST(RunOobCheckedLoad_pseudo3) {
  TestRunOobCheckedLoad_pseudo(A_GIG, false);
  TestRunOobCheckedLoad_pseudo(A_GIG, true);
}

TEST(RunOobCheckedLoad_pseudo4) {
  TestRunOobCheckedLoad_pseudo(2 * A_BILLION, false);
  TestRunOobCheckedLoad_pseudo(2 * A_BILLION, true);
}

TEST(RunOobCheckedLoad_pseudo5) {
  TestRunOobCheckedLoad_pseudo(2 * A_GIG, false);
  TestRunOobCheckedLoad_pseudo(2 * A_GIG, true);
}

TEST(RunOobCheckedLoad_pseudo6) {
  TestRunOobCheckedLoad_pseudo(3 * A_BILLION, false);
  TestRunOobCheckedLoad_pseudo(3 * A_BILLION, true);
}

TEST(RunOobCheckedLoad_pseudo7) {
  TestRunOobCheckedLoad_pseudo(3 * A_GIG, false);
  TestRunOobCheckedLoad_pseudo(3 * A_GIG, true);
}

TEST(RunOobCheckedLoad_pseudo8) {
  TestRunOobCheckedLoad_pseudo(4 * A_BILLION, false);
  TestRunOobCheckedLoad_pseudo(4 * A_BILLION, true);
}

template <typename MemType>
void TestRunOobCheckedLoadT_pseudo(uint64_t x, bool length_is_immediate) {
  const int32_t kReturn = 11999;
  const uint32_t kNumElems = 29;
  MemType buffer[kNumElems];
  uint32_t pseudo_base = static_cast<uint32_t>(x);
  const uint32_t kLength = static_cast<uint32_t>(pseudo_base + sizeof(buffer));

  MemType result;

  RawMachineAssemblerTester<int32_t> m(MachineType::Uint32(),
                                       MachineType::Uint32());
  MachineOperatorBuilder machine(m.zone());
  Node* base = m.PointerConstant(reinterpret_cast<byte*>(buffer) - pseudo_base);
  Node* offset = m.Parameter(0);
  Node* len = length_is_immediate ? m.Int32Constant(kLength) : m.Parameter(1);
  Node* node = m.AddNode(machine.CheckedLoad(MachineTypeForC<MemType>()), base,
                         offset, len);
  Node* store = m.StoreToPointer(
      &result, MachineTypeForC<MemType>().representation(), node);
  USE(store);
  m.Return(m.Int32Constant(kReturn));

  {
    // randomize memory.
    v8::base::RandomNumberGenerator rng;
    rng.SetSeed(103);
    rng.NextBytes(&buffer[0], sizeof(buffer));
  }

  // in-bounds accesses.
  for (uint32_t i = 0; i < kNumElems; i++) {
    uint32_t offset = static_cast<uint32_t>(i * sizeof(MemType));
    MemType expected = buffer[i];
    CHECK_EQ(kReturn, m.Call(offset + pseudo_base, kLength));
    CHECK_EQ(expected, result);
  }

  // slightly out-of-bounds accesses.
  for (uint32_t i = kNumElems; i < kNumElems + 30; i++) {
    uint32_t offset = static_cast<uint32_t>(i * sizeof(MemType));
    CHECK_EQ(kReturn, m.Call(offset + pseudo_base, kLength));
    CheckOobValue(result);
  }

  // way out-of-bounds accesses.
  for (uint64_t i = pseudo_base + sizeof(buffer); i < 0xFFFFFFFF;
       i += A_BILLION) {
    uint32_t offset = static_cast<uint32_t>(i);
    CHECK_EQ(kReturn, m.Call(offset, kLength));
    CheckOobValue(result);
  }
}

TEST(RunOobCheckedLoadT_pseudo0) {
  TestRunOobCheckedLoadT_pseudo<int32_t>(0, false);
  TestRunOobCheckedLoadT_pseudo<int32_t>(0, true);
  TestRunOobCheckedLoadT_pseudo<float>(0, false);
  TestRunOobCheckedLoadT_pseudo<float>(0, true);
  TestRunOobCheckedLoadT_pseudo<double>(0, false);
  TestRunOobCheckedLoadT_pseudo<double>(0, true);
}

TEST(RunOobCheckedLoadT_pseudo1) {
  TestRunOobCheckedLoadT_pseudo<int32_t>(100000, false);
  TestRunOobCheckedLoadT_pseudo<int32_t>(100000, true);
  TestRunOobCheckedLoadT_pseudo<float>(100000, false);
  TestRunOobCheckedLoadT_pseudo<float>(100000, true);
  TestRunOobCheckedLoadT_pseudo<double>(100000, false);
  TestRunOobCheckedLoadT_pseudo<double>(100000, true);
}

TEST(RunOobCheckedLoadT_pseudo2) {
  TestRunOobCheckedLoadT_pseudo<int32_t>(A_BILLION, false);
  TestRunOobCheckedLoadT_pseudo<int32_t>(A_BILLION, true);
  TestRunOobCheckedLoadT_pseudo<float>(A_BILLION, false);
  TestRunOobCheckedLoadT_pseudo<float>(A_BILLION, true);
  TestRunOobCheckedLoadT_pseudo<double>(A_BILLION, false);
  TestRunOobCheckedLoadT_pseudo<double>(A_BILLION, true);
}

TEST(RunOobCheckedLoadT_pseudo3) {
  TestRunOobCheckedLoadT_pseudo<int32_t>(A_GIG, false);
  TestRunOobCheckedLoadT_pseudo<int32_t>(A_GIG, true);
  TestRunOobCheckedLoadT_pseudo<float>(A_GIG, false);
  TestRunOobCheckedLoadT_pseudo<float>(A_GIG, true);
  TestRunOobCheckedLoadT_pseudo<double>(A_GIG, false);
  TestRunOobCheckedLoadT_pseudo<double>(A_GIG, true);
}

TEST(RunOobCheckedLoadT_pseudo4) {
  TestRunOobCheckedLoadT_pseudo<int32_t>(2 * A_BILLION, false);
  TestRunOobCheckedLoadT_pseudo<int32_t>(2 * A_BILLION, true);
  TestRunOobCheckedLoadT_pseudo<float>(2 * A_BILLION, false);
  TestRunOobCheckedLoadT_pseudo<float>(2 * A_BILLION, true);
  TestRunOobCheckedLoadT_pseudo<double>(2 * A_BILLION, false);
  TestRunOobCheckedLoadT_pseudo<double>(2 * A_BILLION, true);
}

TEST(RunOobCheckedLoadT_pseudo5) {
  TestRunOobCheckedLoadT_pseudo<int32_t>(2 * A_GIG, false);
  TestRunOobCheckedLoadT_pseudo<int32_t>(2 * A_GIG, true);
  TestRunOobCheckedLoadT_pseudo<float>(2 * A_GIG, false);
  TestRunOobCheckedLoadT_pseudo<float>(2 * A_GIG, true);
  TestRunOobCheckedLoadT_pseudo<double>(2 * A_GIG, false);
  TestRunOobCheckedLoadT_pseudo<double>(2 * A_GIG, true);
}

TEST(RunOobCheckedLoadT_pseudo6) {
  TestRunOobCheckedLoadT_pseudo<int32_t>(3 * A_BILLION, false);
  TestRunOobCheckedLoadT_pseudo<int32_t>(3 * A_BILLION, true);
  TestRunOobCheckedLoadT_pseudo<float>(3 * A_BILLION, false);
  TestRunOobCheckedLoadT_pseudo<float>(3 * A_BILLION, true);
  TestRunOobCheckedLoadT_pseudo<double>(3 * A_BILLION, false);
  TestRunOobCheckedLoadT_pseudo<double>(3 * A_BILLION, true);
}

TEST(RunOobCheckedLoadT_pseudo7) {
  TestRunOobCheckedLoadT_pseudo<int32_t>(3 * A_GIG, false);
  TestRunOobCheckedLoadT_pseudo<int32_t>(3 * A_GIG, true);
  TestRunOobCheckedLoadT_pseudo<float>(3 * A_GIG, false);
  TestRunOobCheckedLoadT_pseudo<float>(3 * A_GIG, true);
  TestRunOobCheckedLoadT_pseudo<double>(3 * A_GIG, false);
  TestRunOobCheckedLoadT_pseudo<double>(3 * A_GIG, true);
}

TEST(RunOobCheckedLoadT_pseudo8) {
  TestRunOobCheckedLoadT_pseudo<int32_t>(4 * A_BILLION, false);
  TestRunOobCheckedLoadT_pseudo<int32_t>(4 * A_BILLION, true);
  TestRunOobCheckedLoadT_pseudo<float>(4 * A_BILLION, false);
  TestRunOobCheckedLoadT_pseudo<float>(4 * A_BILLION, true);
  TestRunOobCheckedLoadT_pseudo<double>(4 * A_BILLION, false);
  TestRunOobCheckedLoadT_pseudo<double>(4 * A_BILLION, true);
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
