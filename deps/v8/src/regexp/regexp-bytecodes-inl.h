// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_REGEXP_BYTECODES_INL_H_
#define V8_REGEXP_REGEXP_BYTECODES_INL_H_

#include "src/regexp/regexp-bytecodes.h"
// Include the non-inl header before the rest of the headers.

#include <array>
#include <limits>
#include <type_traits>

#include "src/regexp/regexp-macro-assembler.h"  // For StackCheckFlag

namespace v8 {
namespace internal {

template <RegExpBytecodeOperandType>
struct RegExpOperandTypeTraits;

#define DECLARE_BASIC_OPERAND_TYPE_TRAITS(Name, CType)                 \
  template <>                                                          \
  struct RegExpOperandTypeTraits<RegExpBytecodeOperandType::k##Name> { \
    static_assert(!std::is_pointer_v<CType>);                          \
    static constexpr uint8_t kSize = sizeof(CType);                    \
    using kCType = CType;                                              \
    static constexpr bool kIsBasic = true;                             \
  };
BASIC_BYTECODE_OPERAND_TYPE_LIST(DECLARE_BASIC_OPERAND_TYPE_TRAITS)
#undef DECLARE_OPERAND_TYPE_TRAITS

#define DECLARE_SPECIAL_OPERAND_TYPE_TRAITS(Name, Size)                \
  template <>                                                          \
  struct RegExpOperandTypeTraits<RegExpBytecodeOperandType::k##Name> { \
    static constexpr uint8_t kSize = Size;                             \
    static constexpr bool kIsBasic = false;                            \
  };
SPECIAL_BYTECODE_OPERAND_TYPE_LIST(DECLARE_SPECIAL_OPERAND_TYPE_TRAITS)
#undef DECLARE_OPERAND_TYPE_TRAITS

namespace detail {

// Bytecode is 4-byte aligned.
// We can pack operands if multiple operands fit into 4 bytes.
static constexpr int kBytecodeAlignment = 4;

// Calculates packed offsets for each Bytecode operand.
// The first operand can be packed together with the bytecode at an unaligned
// offset 1. All other operands are aligned to their own size if
// they are "basic" types.
template <RegExpBytecodeOperandType... operand_types>
consteval auto CalculatePackedOffsets() {
  constexpr int N = sizeof...(operand_types);
  constexpr std::array<uint8_t, N> kOperandSizes = {
      RegExpOperandTypeTraits<operand_types>::kSize...};
  constexpr std::array<bool, N> kIsBasic = {
      RegExpOperandTypeTraits<operand_types>::kIsBasic...};

  std::array<int, N> offsets{};
  int first_offset = sizeof(RegExpBytecode);
  int offset = first_offset;

  for (size_t i = 0; i < N; ++i) {
    uint8_t operand_size = kOperandSizes[i];

    // An operand is only allowed to be unaligned, if it's packed with the
    // bytecode. All subsequent basic operands must be aligned to their own
    // size.
    if (offset > first_offset && kIsBasic[i]) {
      offset = RoundUp(offset, operand_size);
    }

    // If the operand doesn't fit into the current 4-byte block, start a new
    // 4-byte block.
    if ((offset % kBytecodeAlignment) + operand_size > kBytecodeAlignment) {
      offset = RoundUp<kBytecodeAlignment>(offset);
    }

    offsets[i] = offset;
    offset += operand_size;
  }

  return offsets;
}

template <RegExpBytecodeOperandType... ops>
struct RegExpBytecodeOperandsTraits {
  static constexpr int kOperandCount = sizeof...(ops);
  static constexpr std::array<RegExpBytecodeOperandType, kOperandCount>
      kOperandTypes = {ops...};
  static constexpr std::array<uint8_t, kOperandCount> kOperandSizes = {
      RegExpOperandTypeTraits<ops>::kSize...};
  static constexpr std::array<int, kOperandCount> kOperandOffsets =
      CalculatePackedOffsets<ops...>();
  static constexpr int kSize = RoundUp<kBytecodeAlignment>(
      kOperandCount == 0 ? sizeof(RegExpBytecode)
                         : kOperandOffsets.back() + kOperandSizes.back());
};

template <RegExpBytecode bc>
struct RegExpBytecodeOperandNames;

#define DECLARE_OPERAND_NAMES(CamelName, SnakeName, OpNames, OpTypes) \
  template <>                                                         \
  struct RegExpBytecodeOperandNames<RegExpBytecode::k##CamelName> {   \
    enum class Operand { UNPAREN(OpNames) };                          \
    using enum Operand;                                               \
  };
REGEXP_BYTECODE_LIST(DECLARE_OPERAND_NAMES)
#undef DECLARE_OPERAND_NAMES

template <RegExpBytecode bc, RegExpBytecodeOperandType... OpTypes>
class RegExpBytecodeOperandsBase {
 public:
  using Operand = RegExpBytecodeOperandNames<bc>::Operand;
  using Traits = RegExpBytecodeOperandsTraits<OpTypes...>;
  static constexpr int kCount = Traits::kOperandCount;
  static constexpr int kTotalSize = Traits::kSize;
  static consteval int Index(Operand op) { return static_cast<uint8_t>(op); }
  static consteval int Size(Operand op) {
    return Traits::kOperandSizes[Index(op)];
  }
  static consteval int Offset(Operand op) {
    return Traits::kOperandOffsets[Index(op)];
  }
  static consteval RegExpBytecodeOperandType Type(Operand op) {
    return Traits::kOperandTypes[Index(op)];
  }

  // Calls |f| templatized by Operand for each Operand in the Operands list.
  // Example:
  // using Operands = RegExpBytecodeOperands<RegExpBytecode::...>;
  // size_t op_sizes = 0;
  // Operands::ForEachOperand([]<auto op>() {
  //   op_sizes += Operands::Size(op);
  // });
  // Note that this gets evaluated at compile time, so op_sizes in the example
  // above is essentially a constant.
  template <typename Func>
  static constexpr void ForEachOperand(Func&& f) {
    [&]<size_t... I>(std::index_sequence<I...>) {
      (..., f.template operator()<static_cast<Operand>(I)>());
    }(std::make_index_sequence<kCount>{});
  }

  // Similar to above, but calls |f| only for operands of a given type.
  template <RegExpBytecodeOperandType OpType, typename Func>
  static constexpr void ForEachOperandOfType(Func&& f) {
    ForEachOperand([&]<auto operand>() {
      if constexpr (Type(operand) == OpType) {
        f.template operator()<operand>();
      }
    });
  }

 private:
  template <RegExpBytecodeOperandType OperandType>
    requires(RegExpOperandTypeTraits<OperandType>::kIsBasic)
  static auto GetAligned(const uint8_t* pc, int offset) {
    DCHECK_EQ(*pc, RegExpBytecodes::ToByte(bc));
    using CType = RegExpOperandTypeTraits<OperandType>::kCType;
    DCHECK(IsAligned(offset, sizeof(CType)));
    return *reinterpret_cast<const CType*>(pc + offset);
  }

  // TODO(pthier): We can remove unaligned packing once we have fully switched
  // to the new bytecode layout. This is for backwards-compatibility with the
  // old layout only.
  template <RegExpBytecodeOperandType OperandType>
    requires(RegExpOperandTypeTraits<OperandType>::kIsBasic)
  static auto GetPacked(const uint8_t* pc, int offset) {
    DCHECK_EQ(*pc, RegExpBytecodes::ToByte(bc));
    // Only unaligned packing of 2-byte values with the bytecode is supported.
    DCHECK_EQ(offset, 1);
    static_assert(RegExpOperandTypeTraits<OperandType>::kSize == 2);
    using CType = RegExpOperandTypeTraits<OperandType>::kCType;
    DCHECK(!IsAligned(offset, sizeof(CType)));
    int32_t packed_value = *reinterpret_cast<const int32_t*>(pc);
    return static_cast<CType>(packed_value >> BYTECODE_SHIFT);
  }

 public:
  template <Operand op>
    requires(RegExpOperandTypeTraits<Type(op)>::kIsBasic)
  static auto Get(const uint8_t* pc) {
    constexpr RegExpBytecodeOperandType OperandType = Type(op);
    constexpr int offset = Offset(op);
    using CType = RegExpOperandTypeTraits<OperandType>::kCType;
    // TODO(pthier): We can remove unaligned packing once we have fully switched
    // to the new bytecode layout. This is for backwards-compatibility with the
    // old layout only.
    if constexpr (!IsAligned(offset, sizeof(CType))) {
      return GetPacked<OperandType>(pc, offset);
    } else {
      return GetAligned<OperandType>(pc, offset);
    }
  }

  template <Operand op>
    requires(Type(op) == RegExpBytecodeOperandType::kBitTable)
  static auto Get(const uint8_t* pc) {
    DCHECK_EQ(*pc, RegExpBytecodes::ToByte(bc));
    constexpr int offset = Offset(op);
    return pc + offset;
  }
};

}  // namespace detail

#define PACK_OPTIONAL(x, ...) x __VA_OPT__(, ) __VA_ARGS__

#define DECLARE_OPERANDS(CamelName, SnakeName, OpNames, OpTypes)   \
  template <>                                                      \
  class RegExpBytecodeOperands<RegExpBytecode::k##CamelName> final \
      : public detail::RegExpBytecodeOperandsBase<PACK_OPTIONAL(   \
            RegExpBytecode::k##CamelName, UNPAREN(OpTypes))>,      \
        public AllStatic {                                         \
   public:                                                         \
    using enum Operand;                                            \
  };

REGEXP_BYTECODE_LIST(DECLARE_OPERANDS)
#undef DECLARE_OPERANDS

namespace detail {

#define DECLARE_BYTECODE_NAMES(CamelName, ...) #CamelName,
static constexpr const char* kBytecodeNames[] = {
    REGEXP_BYTECODE_LIST(DECLARE_BYTECODE_NAMES)};
#undef DECLARE_BYTECODE_NAMES

#define DECLARE_BYTECODE_SIZES(CamelName, ...) \
  RegExpBytecodeOperands<RegExpBytecode::k##CamelName>::kTotalSize,
static constexpr uint8_t kBytecodeSizes[] = {
    REGEXP_BYTECODE_LIST(DECLARE_BYTECODE_SIZES)};
#undef DECLARE_BYTECODE_SIZES

}  // namespace detail

// static
template <typename Func>
decltype(auto) RegExpBytecodes::DispatchOnBytecode(RegExpBytecode bytecode,
                                                   Func&& f) {
  switch (bytecode) {
#define CASE(CamelName, ...)         \
  case RegExpBytecode::k##CamelName: \
    return f.template operator()<RegExpBytecode::k##CamelName>();
    REGEXP_BYTECODE_LIST(CASE)
#undef CASE
  }
  UNREACHABLE();
}

// static
constexpr const char* RegExpBytecodes::Name(RegExpBytecode bytecode) {
  return Name(ToByte(bytecode));
}

// static
constexpr const char* RegExpBytecodes::Name(uint8_t bytecode) {
  DCHECK_LT(bytecode, kCount);
  return detail::kBytecodeNames[bytecode];
}

// static
constexpr uint8_t RegExpBytecodes::Size(RegExpBytecode bytecode) {
  return Size(ToByte(bytecode));
}

// static
constexpr uint8_t RegExpBytecodes::Size(uint8_t bytecode) {
  DCHECK_LT(bytecode, kCount);
  return detail::kBytecodeSizes[bytecode];
}

// Checks for backwards compatibility.
// TODO(pthier): Remove once we removed the old bytecode format.
static_assert(kRegExpBytecodeCount == RegExpBytecodes::kCount);

#define CHECK_BYTECODE_VALUE(CamelName, SnakeName, ...)                  \
  static_assert(RegExpBytecodes::ToByte(RegExpBytecode::k##CamelName) == \
                BC_##SnakeName);
REGEXP_BYTECODE_LIST(CHECK_BYTECODE_VALUE)
#undef CHECK_BYTECODE_VALUE

#define CHECK_LENGTH(CamelName, SnakeName, ...)                        \
  static_assert(RegExpBytecodes::Size(RegExpBytecode::k##CamelName) == \
                RegExpBytecodeLength(BC_##SnakeName));
REGEXP_BYTECODE_LIST(CHECK_LENGTH)
#undef CHECK_LENGTH

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_REGEXP_BYTECODES_INL_H_
