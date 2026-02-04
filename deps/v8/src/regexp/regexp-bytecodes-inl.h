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

#define DECLARE_BASIC_OPERAND_TYPE_TRAITS(Name, CType)                      \
  template <>                                                               \
  struct RegExpOperandTypeTraits<RegExpBytecodeOperandType::k##Name> {      \
    static_assert(!std::is_pointer_v<CType>);                               \
    static constexpr uint8_t kSize = sizeof(CType);                         \
    using kCType = CType;                                                   \
    static constexpr bool kIsBasic = true;                                  \
    static constexpr kCType kMinValue = std::numeric_limits<kCType>::min(); \
    static constexpr kCType kMaxValue = std::numeric_limits<kCType>::max(); \
    static constexpr size_t kAlignment = kSize;                             \
  };
BASIC_BYTECODE_OPERAND_TYPE_LIST(DECLARE_BASIC_OPERAND_TYPE_TRAITS)
#undef DECLARE_OPERAND_TYPE_TRAITS

#define DECLARE_BASIC_OPERAND_TYPE_LIMITS_TRAITS(Name, CType, MinValue, \
                                                 MaxValue)              \
  template <>                                                           \
  struct RegExpOperandTypeTraits<RegExpBytecodeOperandType::k##Name> {  \
    static_assert(!std::is_pointer_v<CType>);                           \
    static constexpr uint8_t kSize = sizeof(CType);                     \
    using kCType = CType;                                               \
    static constexpr bool kIsBasic = true;                              \
    static_assert(std::is_enum_v<kCType> ||                             \
                  MinValue >= std::numeric_limits<kCType>::min());      \
    static_assert(std::is_enum_v<kCType> ||                             \
                  MaxValue <= std::numeric_limits<kCType>::max());      \
    static constexpr kCType kMinValue = MinValue;                       \
    static constexpr kCType kMaxValue = MaxValue;                       \
    static constexpr size_t kAlignment = kSize;                         \
  };
BASIC_BYTECODE_OPERAND_TYPE_LIMITS_LIST(
    DECLARE_BASIC_OPERAND_TYPE_LIMITS_TRAITS)
#undef DECLARE_OPERAND_TYPE_LIMITS_TRAITS

#define DECLARE_SPECIAL_OPERAND_TYPE_TRAITS(Name, Size, Alignment)     \
  template <>                                                          \
  struct RegExpOperandTypeTraits<RegExpBytecodeOperandType::k##Name> { \
    static constexpr uint8_t kSize = Size;                             \
    static constexpr bool kIsBasic = false;                            \
    static constexpr size_t kAlignment = Alignment;                    \
    static_assert(IsAligned(kSize, kAlignment));                       \
  };
SPECIAL_BYTECODE_OPERAND_TYPE_LIST(DECLARE_SPECIAL_OPERAND_TYPE_TRAITS)
#undef DECLARE_OPERAND_TYPE_TRAITS

namespace detail {

// Calculates packed offsets for each Bytecode operand.
// All operands are aligned to their own size.
template <RegExpBytecodeOperandType... operand_types>
consteval auto CalculateAlignedOffsets() {
  constexpr int N = sizeof...(operand_types);
  constexpr std::array<uint8_t, N> kOperandSizes = {
      RegExpOperandTypeTraits<operand_types>::kSize...};
  constexpr std::array<uint8_t, N> kOperandAlignments = {
      RegExpOperandTypeTraits<operand_types>::kAlignment...};

  std::array<int, N> offsets{};
  int first_offset = sizeof(RegExpBytecode);
  int offset = first_offset;

  for (size_t i = 0; i < N; ++i) {
    uint8_t operand_size = kOperandSizes[i];
    size_t operand_alignment = kOperandAlignments[i];

    offset = RoundUp(offset, operand_alignment);

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
  static constexpr std::array<uint8_t, kOperandCount> kOperandAlignments = {
      RegExpOperandTypeTraits<ops>::kAlignment...};
  static constexpr std::array<int, kOperandCount> kOperandOffsets =
      CalculateAlignedOffsets<ops...>();
  static constexpr int kSize = RoundUp<kBytecodeAlignment>(
      kOperandCount == 0 ? sizeof(RegExpBytecode)
                         : kOperandOffsets.back() + kOperandSizes.back());
};

template <RegExpBytecode bc>
struct RegExpBytecodeOperandNames;

#define DECLARE_OPERAND_NAMES(CamelName, OpNames, OpTypes)          \
  template <>                                                       \
  struct RegExpBytecodeOperandNames<RegExpBytecode::k##CamelName> { \
    enum class Operand { UNPAREN(OpNames) };                        \
    using enum Operand;                                             \
  };
REGEXP_BYTECODE_LIST(DECLARE_OPERAND_NAMES)
#undef DECLARE_OPERAND_NAMES

template <RegExpBytecode bc, RegExpBytecodeOperandType... OpTypes>
class RegExpBytecodeOperandsBase {
 public:
  static constexpr RegExpBytecode kBytecode = bc;
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

  // Returns a tuple of all operands.
  static consteval auto GetOperandsTuple() {
    return []<size_t... Is>(std::index_sequence<Is...>) {
      return std::tuple_cat([]<size_t I>() {
        constexpr auto id = static_cast<Operand>(I);
        return std::tuple(std::integral_constant<Operand, id>{});
      }.template operator()<Is>()...);
    }(std::make_index_sequence<kCount>{});
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
    constexpr auto filtered_ops = GetOperandsTuple();
    std::apply([&](auto... ops) { (..., f.template operator()<ops.value>()); },
               filtered_ops);
  }

  // Similar to ForEachOperand, but additionally provides the current index as
  // a template argument. The index is a sequential index of operands.
  template <typename Func>
  static constexpr void ForEachOperandWithIndex(Func&& f) {
    constexpr auto filtered_ops = GetOperandsTuple();
    [&]<size_t... I>(std::index_sequence<I...>) {
      (...,
       f.template operator()<
           std::tuple_element_t<I, decltype(filtered_ops)>::value /* Operand */,
           I /* Index */>());
    }(std::make_index_sequence<std::tuple_size_v<decltype(filtered_ops)>>{});
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

 public:
  template <Operand op>
    requires(RegExpOperandTypeTraits<Type(op)>::kIsBasic)
  static auto Get(const uint8_t* pc, const DisallowGarbageCollection& no_gc) {
    DCHECK_EQ(RegExpBytecodes::FromPtr(pc), bc);
    constexpr RegExpBytecodeOperandType OperandType = Type(op);
    constexpr int offset = Offset(op);
    using CType = RegExpOperandTypeTraits<OperandType>::kCType;
    DCHECK(IsAligned(offset, sizeof(CType)));
    return *reinterpret_cast<const CType*>(pc + offset);
  }

  template <Operand op>
    requires(RegExpOperandTypeTraits<Type(op)>::kIsBasic)
  static auto Get(DirectHandle<TrustedByteArray> bytecode, int offset,
                  Zone* zone) {
    // Basic operand types won't allocate, so we can always fallback to the
    // GC-unsafe version.
    DisallowGarbageCollection no_gc;
    return Get<op>(bytecode->begin() + offset, no_gc);
  }

  template <Operand op>
    requires(Type(op) == RegExpBytecodeOperandType::kBitTable)
  static auto Get(const uint8_t* pc, const DisallowGarbageCollection& no_gc) {
    static_assert(Size(op) == RegExpMacroAssembler::kTableSize / kBitsPerByte);
    DCHECK_EQ(RegExpBytecodes::FromPtr(pc), bc);
    constexpr int offset = Offset(op);
    return pc + offset;
  }

  template <Operand op>
    requires(Type(op) == RegExpBytecodeOperandType::kBitTable)
  static auto Get(DirectHandle<TrustedByteArray> bytecode, int offset,
                  Zone* zone) {
    static_assert(Size(op) == RegExpMacroAssembler::kTableSize / kBitsPerByte);
    DCHECK_EQ(RegExpBytecodes::FromPtr(bytecode->begin() + offset), bc);
    constexpr int op_offset = Offset(op);
    const uint8_t* start = bytecode->begin() + offset + op_offset;
    const uint8_t* end = start + Size(op);
    return ZoneVector<uint8_t>(start, end, zone);
  }
};

}  // namespace detail

#define PACK_OPTIONAL(x, ...) x __VA_OPT__(, ) __VA_ARGS__

#define DECLARE_OPERANDS(CamelName, OpNames, OpTypes)              \
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

#define DECLARE_OPERAND_TYPE_SIZE(Name, ...) \
  RegExpOperandTypeTraits<RegExpBytecodeOperandType::k##Name>::kSize,
static constexpr uint8_t kOperandTypeSizes[] = {
    BYTECODE_OPERAND_TYPE_LIST(DECLARE_OPERAND_TYPE_SIZE)};
#undef DECLARE_OPERAND_TYPE_SIZE

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

// static
constexpr uint8_t RegExpBytecodes::Size(RegExpBytecodeOperandType type) {
  return detail::kOperandTypeSizes[static_cast<int>(type)];
}

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_REGEXP_BYTECODES_INL_H_
