// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_MACHINE_LOWERING_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_MACHINE_LOWERING_REDUCER_H_

#include "src/base/v8-fallthrough.h"
#include "src/common/globals.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/globals.h"
#include "src/compiler/simplified-operator.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/index.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/optimization-phase.h"
#include "src/compiler/turboshaft/reducer-traits.h"
#include "src/compiler/turboshaft/representations.h"
#include "src/objects/bigint.h"
#include "src/objects/heap-number.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

struct MachineLoweringReducerArgs {
  Factory* factory;
};

// MachineLoweringReducer, formerly known as EffectControlLinearizer, lowers
// simplified operations to machine operations.
template <typename Next>
class MachineLoweringReducer : public Next {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE()

  using ArgT =
      base::append_tuple_type<typename Next::ArgT, MachineLoweringReducerArgs>;

  template <typename... Args>
  explicit MachineLoweringReducer(const std::tuple<Args...>& args)
      : Next(args),
        factory_(std::get<MachineLoweringReducerArgs>(args).factory) {}

  bool NeedsHeapObjectCheck(ObjectIsOp::InputAssumptions input_assumptions) {
    // TODO(nicohartmann@): Consider type information once we have that.
    switch (input_assumptions) {
      case ObjectIsOp::InputAssumptions::kNone:
        return true;
      case ObjectIsOp::InputAssumptions::kHeapObject:
      case ObjectIsOp::InputAssumptions::kBigInt:
        return false;
    }
  }

  V<Word32> ReduceObjectIs(V<Tagged> input, ObjectIsOp::Kind kind,
                           ObjectIsOp::InputAssumptions input_assumptions) {
    switch (kind) {
      case ObjectIsOp::Kind::kBigInt:
      case ObjectIsOp::Kind::kBigInt64: {
        DCHECK_IMPLIES(kind == ObjectIsOp::Kind::kBigInt64, Is64());

        Label<Word32> done(this);

        if (input_assumptions != ObjectIsOp::InputAssumptions::kBigInt) {
          if (NeedsHeapObjectCheck(input_assumptions)) {
            // Check for Smi.
            GOTO_IF(IsSmi(input), done, 0);
          }

          // Check for BigInt.
          V<Tagged> map = LoadField<Tagged>(input, AccessBuilder::ForMap());
          V<Word32> is_bigint_map =
              __ TaggedEqual(map, __ HeapConstant(factory_->bigint_map()));
          GOTO_IF_NOT(is_bigint_map, done, 0);
        }

        if (kind == ObjectIsOp::Kind::kBigInt) {
          GOTO(done, 1);
        } else {
          DCHECK_EQ(kind, ObjectIsOp::Kind::kBigInt64);
          // We have to perform check for BigInt64 range.
          V<Word32> bitfield =
              LoadField<Word32>(input, AccessBuilder::ForBigIntBitfield());
          GOTO_IF(__ Word32Equal(bitfield, 0), done, 1);

          // Length must be 1.
          V<Word32> length_field =
              __ Word32BitwiseAnd(bitfield, BigInt::LengthBits::kMask);
          GOTO_IF_NOT(__ Word32Equal(length_field,
                                     uint32_t{1} << BigInt::LengthBits::kShift),
                      done, 0);

          // Check if it fits in 64 bit signed int.
          V<Word64> lsd = LoadField<Word64>(
              input, AccessBuilder::ForBigIntLeastSignificantDigit64());
          V<Word32> magnitude_check = __ Uint64LessThanOrEqual(
              lsd, std::numeric_limits<int64_t>::max());
          GOTO_IF(magnitude_check, done, 1);

          // The BigInt probably doesn't fit into signed int64. The only
          // exception is int64_t::min. We check for this.
          V<Word32> sign =
              __ Word32BitwiseAnd(bitfield, BigInt::SignBits::kMask);
          V<Word32> sign_check = __ Word32Equal(sign, BigInt::SignBits::kMask);
          GOTO_IF_NOT(sign_check, done, 0);

          V<Word32> min_check =
              __ Word64Equal(lsd, std::numeric_limits<int64_t>::min());
          GOTO_IF(min_check, done, 1);

          GOTO(done, 0);
        }

        BIND(done, result);
        return result;
      }
      case ObjectIsOp::Kind::kCallable:
      case ObjectIsOp::Kind::kConstructor:
      case ObjectIsOp::Kind::kDetectableCallable:
      case ObjectIsOp::Kind::kNonCallable:
      case ObjectIsOp::Kind::kReceiver:
      case ObjectIsOp::Kind::kUndetectable: {
        Label<Word32> done(this);

        // Check for Smi if necessary.
        if (NeedsHeapObjectCheck(input_assumptions)) {
          GOTO_IF(IsSmi(input), done, 0);
        }

        // Load bitfield from map.
        V<Tagged> map = LoadField<Tagged>(input, AccessBuilder::ForMap());
        V<Word32> bitfield =
            LoadField<Word32>(map, AccessBuilder::ForMapBitField());

        V<Word32> check;
        switch (kind) {
          case ObjectIsOp::Kind::kCallable:
            check =
                __ Word32Equal(Map::Bits1::IsCallableBit::kMask,
                               __ Word32BitwiseAnd(
                                   bitfield, Map::Bits1::IsCallableBit::kMask));
            break;
          case ObjectIsOp::Kind::kConstructor:
            check = __ Word32Equal(
                Map::Bits1::IsConstructorBit::kMask,
                __ Word32BitwiseAnd(bitfield,
                                    Map::Bits1::IsConstructorBit::kMask));
            break;
          case ObjectIsOp::Kind::kDetectableCallable:
            check = __ Word32Equal(
                Map::Bits1::IsCallableBit::kMask,
                __ Word32BitwiseAnd(
                    bitfield, (Map::Bits1::IsCallableBit::kMask) |
                                  (Map::Bits1::IsUndetectableBit::kMask)));
            break;
          case ObjectIsOp::Kind::kNonCallable:
            check = __ Word32Equal(
                0, __ Word32BitwiseAnd(bitfield,
                                       Map::Bits1::IsCallableBit::kMask));
            GOTO_IF_NOT(check, done, 0);
            // Fallthrough into receiver check.
            V8_FALLTHROUGH;
          case ObjectIsOp::Kind::kReceiver: {
            static_assert(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
            V<Word32> instance_type =
                LoadField<Word32>(map, AccessBuilder::ForMapInstanceType());
            check =
                __ Uint32LessThanOrEqual(FIRST_JS_RECEIVER_TYPE, instance_type);
            break;
          }
          case ObjectIsOp::Kind::kUndetectable:
            check = __ Word32Equal(
                Map::Bits1::IsUndetectableBit::kMask,
                __ Word32BitwiseAnd(bitfield,
                                    Map::Bits1::IsUndetectableBit::kMask));
            break;
          default:
            UNREACHABLE();
        }
        GOTO(done, check);

        BIND(done, result);
        return result;
      }
      case ObjectIsOp::Kind::kSmi: {
        // If we statically know that this is a heap object, it cannot be a Smi.
        if (!NeedsHeapObjectCheck(input_assumptions)) {
          return __ Word32Constant(0);
        }
        return IsSmi(input);
      }
      case ObjectIsOp::Kind::kNumber: {
        Label<Word32> done(this);

        // Check for Smi if necessary.
        if (NeedsHeapObjectCheck(input_assumptions)) {
          GOTO_IF(IsSmi(input), done, 1);
        }

        V<Tagged> map = LoadField<Tagged>(input, AccessBuilder::ForMap());
        GOTO(done,
             __ TaggedEqual(map, __ HeapConstant(factory_->heap_number_map())));

        BIND(done, result);
        return result;
      }
      case ObjectIsOp::Kind::kSymbol:
      case ObjectIsOp::Kind::kString:
      case ObjectIsOp::Kind::kArrayBufferView: {
        Label<Word32> done(this);

        // Check for Smi if necessary.
        if (NeedsHeapObjectCheck(input_assumptions)) {
          GOTO_IF(IsSmi(input), done, 0);
        }

        // Load instance type from map.
        V<Tagged> map = LoadField<Tagged>(input, AccessBuilder::ForMap());
        V<Word32> instance_type =
            LoadField<Word32>(map, AccessBuilder::ForMapInstanceType());

        V<Word32> check;
        switch (kind) {
          case ObjectIsOp::Kind::kSymbol:
            check = __ Word32Equal(instance_type, SYMBOL_TYPE);
            break;
          case ObjectIsOp::Kind::kString:
            check = __ Uint32LessThan(instance_type, FIRST_NONSTRING_TYPE);
            break;
          case ObjectIsOp::Kind::kArrayBufferView:
            check = __ Uint32LessThan(
                __ Word32Sub(instance_type, FIRST_JS_ARRAY_BUFFER_VIEW_TYPE),
                LAST_JS_ARRAY_BUFFER_VIEW_TYPE -
                    FIRST_JS_ARRAY_BUFFER_VIEW_TYPE + 1);
            break;
          default:
            UNREACHABLE();
        }
        GOTO(done, check);

        BIND(done, result);
        return result;
      }
    }

    UNREACHABLE();
  }

  OpIndex ReduceConvertToObject(
      OpIndex input, ConvertToObjectOp::Kind kind,
      RegisterRepresentation input_rep,
      ConvertToObjectOp::InputInterpretation input_interpretation,
      CheckForMinusZeroMode minus_zero_mode) {
    switch (kind) {
      case ConvertToObjectOp::Kind::kBigInt: {
        DCHECK(Is64());
        DCHECK_EQ(input_rep, RegisterRepresentation::Word64());
        Label<Tagged> done(this);

        // BigInts with value 0 must be of size 0 (canonical form).
        GOTO_IF(__ Word64Equal(input, int64_t{0}), done,
                AllocateBigInt(OpIndex::Invalid(), OpIndex::Invalid()));

        if (input_interpretation ==
            ConvertToObjectOp::InputInterpretation::kSigned) {
          // Shift sign bit into BigInt's sign bit position.
          V<Word32> bitfield = __ Word32BitwiseOr(
              BigInt::LengthBits::encode(1),
              __ Word64ShiftRightLogical(
                  input, static_cast<int64_t>(63 - BigInt::SignBits::kShift)));

          // We use (value XOR (value >> 63)) - (value >> 63) to compute the
          // absolute value, in a branchless fashion.
          V<Word64> sign_mask =
              __ Word64ShiftRightArithmetic(input, int64_t{63});
          V<Word64> absolute_value =
              __ Word64Sub(__ Word64BitwiseXor(input, sign_mask), sign_mask);
          GOTO(done, AllocateBigInt(bitfield, absolute_value));
        } else {
          DCHECK_EQ(input_interpretation,
                    ConvertToObjectOp::InputInterpretation::kUnsigned);
          const auto bitfield = BigInt::LengthBits::encode(1);
          GOTO(done, AllocateBigInt(__ Word32Constant(bitfield), input));
        }
        BIND(done, result);
        return result;
      }
      case ConvertToObjectOp::Kind::kNumber: {
        if (input_rep == RegisterRepresentation::Word32()) {
          switch (input_interpretation) {
            case ConvertToObjectOp::InputInterpretation::kSigned: {
              if (SmiValuesAre32Bits()) {
                return __ SmiTag(input);
              }
              DCHECK(SmiValuesAre31Bits());

              Label<Tagged> done(this);
              Label<> overflow(this);

              SmiTagOrOverflow(input, &overflow, &done);

              if (BIND(overflow)) {
                GOTO(done, AllocateHeapNumberWithValue(
                               __ ChangeInt32ToFloat64(input)));
              }

              BIND(done, result);
              return result;
            }
            case ConvertToObjectOp::InputInterpretation::kUnsigned: {
              Label<Tagged> done(this);

              GOTO_IF(__ Uint32LessThanOrEqual(input, Smi::kMaxValue), done,
                      __ SmiTag(input));
              GOTO(done, AllocateHeapNumberWithValue(
                             __ ChangeUint32ToFloat64(input)));

              BIND(done, result);
              return result;
            }
            case ConvertToObjectOp::InputInterpretation::kCharCode:
            case ConvertToObjectOp::InputInterpretation::kCodePoint:
              UNREACHABLE();
          }
        } else if (input_rep == RegisterRepresentation::Word64()) {
          switch (input_interpretation) {
            case ConvertToObjectOp::InputInterpretation::kSigned: {
              Label<Tagged> done(this);
              Label<> outside_smi_range(this);

              V<Word32> v32 = input;
              V<Word64> v64 = __ ChangeInt32ToInt64(v32);
              GOTO_IF_NOT(__ Word64Equal(v64, input), outside_smi_range);

              if constexpr (SmiValuesAre32Bits()) {
                GOTO(done, __ SmiTag(input));
              } else {
                SmiTagOrOverflow(v32, &outside_smi_range, &done);
              }

              if (BIND(outside_smi_range)) {
                GOTO(done, AllocateHeapNumberWithValue(
                               __ ChangeInt64ToFloat64(input)));
              }

              BIND(done, result);
              return result;
            }
            case ConvertToObjectOp::InputInterpretation::kUnsigned: {
              Label<Tagged> done(this);

              GOTO_IF(__ Uint64LessThanOrEqual(input, Smi::kMaxValue), done,
                      __ SmiTag(input));
              GOTO(done,
                   AllocateHeapNumberWithValue(__ ChangeInt64ToFloat64(input)));

              BIND(done, result);
              return result;
            }
            case ConvertToObjectOp::InputInterpretation::kCharCode:
            case ConvertToObjectOp::InputInterpretation::kCodePoint:
              UNREACHABLE();
          }
        } else {
          DCHECK_EQ(input_rep, RegisterRepresentation::Float64());
          Label<Tagged> done(this);
          Label<> outside_smi_range(this);

          V<Word32> v32 = __ TruncateFloat64ToInt32OverflowUndefined(input);
          GOTO_IF_NOT(__ Float64Equal(input, __ ChangeInt32ToFloat64(v32)),
                      outside_smi_range);

          if (minus_zero_mode == CheckForMinusZeroMode::kCheckForMinusZero) {
            // In case of 0, we need to check the high bits for the IEEE -0
            // pattern.
            IF(__ Word32Equal(v32, 0)) {
              GOTO_IF(__ Int32LessThan(__ Float64ExtractHighWord32(input), 0),
                      outside_smi_range);
            }
            END_IF
          }

          if constexpr (SmiValuesAre32Bits()) {
            GOTO(done, __ SmiTag(v32));
          } else {
            SmiTagOrOverflow(v32, &outside_smi_range, &done);
          }

          if (BIND(outside_smi_range)) {
            GOTO(done, AllocateHeapNumberWithValue(input));
          }

          BIND(done, result);
          return result;
        }
        UNREACHABLE();
        break;
      }
      case ConvertToObjectOp::Kind::kHeapNumber: {
        DCHECK_EQ(input_rep, RegisterRepresentation::Float64());
        DCHECK_EQ(input_interpretation,
                  ConvertToObjectOp::InputInterpretation::kSigned);
        return AllocateHeapNumberWithValue(input);
      }
      case ConvertToObjectOp::Kind::kSmi: {
        DCHECK_EQ(input_rep, RegisterRepresentation::Word32());
        DCHECK_EQ(input_interpretation,
                  ConvertToObjectOp::InputInterpretation::kSigned);
        return __ SmiTag(input);
      }
      case ConvertToObjectOp::Kind::kBoolean: {
        DCHECK_EQ(input_rep, RegisterRepresentation::Word32());
        DCHECK_EQ(input_interpretation,
                  ConvertToObjectOp::InputInterpretation::kSigned);
        Label<Tagged> done(this);

        IF(input) { GOTO(done, __ HeapConstant(factory_->true_value())); }
        ELSE { GOTO(done, __ HeapConstant(factory_->false_value())); }
        END_IF

        BIND(done, result);
        return result;
      }
      case ConvertToObjectOp::Kind::kString: {
        Label<Word32> single_code(this);
        Label<Tagged> done(this);

        if (input_interpretation ==
            ConvertToObjectOp::InputInterpretation::kCharCode) {
          GOTO(single_code, __ Word32BitwiseAnd(input, 0xFFFF));
        } else {
          DCHECK_EQ(input_interpretation,
                    ConvertToObjectOp::InputInterpretation::kCodePoint);
          // Check if the input is a single code unit.
          GOTO_IF_LIKELY(__ Uint32LessThanOrEqual(input, 0xFFFF), single_code,
                         input);

          // Generate surrogate pair string.

          // Convert UTF32 to UTF16 code units and store as a 32 bit word.
          V<Word32> lead_offset = __ Word32Constant(0xD800 - (0x10000 >> 10));

          // lead = (codepoint >> 10) + LEAD_OFFSET
          V<Word32> lead =
              __ Word32Add(__ Word32ShiftRightLogical(input, 10), lead_offset);

          // trail = (codepoint & 0x3FF) + 0xDC00
          V<Word32> trail =
              __ Word32Add(__ Word32BitwiseAnd(input, 0x3FF), 0xDC00);

          // codepoint = (trail << 16) | lead
#if V8_TARGET_BIG_ENDIAN
          V<Word32> code =
              __ Word32BitwiseOr(__ Word32ShiftLeft(lead, 16), trail);
#else
          V<Word32> code =
              __ Word32BitwiseOr(__ Word32ShiftLeft(trail, 16), lead);
#endif

          // Allocate a new SeqTwoByteString for {code}.
          V<Tagged> string =
              __ Allocate(__ IntPtrConstant(SeqTwoByteString::SizeFor(2)),
                          AllocationType::kYoung, AllowLargeObjects::kFalse);
          // Set padding to 0.
          __ Store(string, __ IntPtrConstant(0),
                   StoreOp::Kind::Aligned(BaseTaggedness::kTaggedBase),
                   MemoryRepresentation::TaggedSigned(), kNoWriteBarrier,
                   SeqTwoByteString::SizeFor(2) - kObjectAlignment);
          StoreField(string, AccessBuilder::ForMap(),
                     __ HeapConstant(factory_->string_map()));
          StoreField(string, AccessBuilder::ForNameRawHashField(),
                     __ Word32Constant(Name::kEmptyHashField));
          StoreField(string, AccessBuilder::ForStringLength(),
                     __ Word32Constant(2));
          __ Store(string, code,
                   StoreOp::Kind::Aligned(BaseTaggedness::kTaggedBase),
                   MemoryRepresentation::Uint32(), kNoWriteBarrier,
                   SeqTwoByteString::kHeaderSize);
          GOTO(done, string);
        }

        if (BIND(single_code, code)) {
          // Check if the {code} is a one byte character.
          IF_LIKELY(
              __ Uint32LessThanOrEqual(code, String::kMaxOneByteCharCode)) {
            // Load the isolate wide single character string table.
            OpIndex table =
                __ HeapConstant(factory_->single_character_string_table());

            // Compute the {table} index for {code}.
            V<WordPtr> index = __ ChangeUint32ToUintPtr(code);

            // Load the string for the {code} from the single character string
            // table.
            OpIndex entry = LoadElement(
                table, AccessBuilder::ForFixedArrayElement(), index);

            // Use the {entry} from the {table}.
            GOTO(done, entry);
          }
          ELSE {
            // Allocate a new SeqTwoBytesString for {code}.
            V<Tagged> string =
                __ Allocate(__ IntPtrConstant(SeqTwoByteString::SizeFor(1)),
                            AllocationType::kYoung, AllowLargeObjects::kFalse);

            // Set padding to 0.
            __ Store(string, __ IntPtrConstant(0),
                     StoreOp::Kind::Aligned(BaseTaggedness::kTaggedBase),
                     MemoryRepresentation::TaggedSigned(), kNoWriteBarrier,
                     SeqTwoByteString::SizeFor(1) - kObjectAlignment);
            StoreField(string, AccessBuilder::ForMap(),
                       __ HeapConstant(factory_->string_map()));
            StoreField(string, AccessBuilder::ForNameRawHashField(),
                       __ Word32Constant(Name::kEmptyHashField));
            StoreField(string, AccessBuilder::ForStringLength(),
                       __ Word32Constant(1));
            __ Store(string, code,
                     StoreOp::Kind::Aligned(BaseTaggedness::kTaggedBase),
                     MemoryRepresentation::Uint16(), kNoWriteBarrier,
                     SeqTwoByteString::kHeaderSize);
            GOTO(done, string);
          }
          END_IF
        }

        BIND(done, result);
        return result;
      }
    }

    UNREACHABLE();
  }

  // TODO(nicohartmann@): Remove this once ECL has been fully ported.
  // ECL: ChangeInt64ToSmi(input) ==> MLR: __ SmiTag(input)
  // ECL: ChangeInt32ToSmi(input) ==> MLR: __ SmiTag(input)
  // ECL: ChangeUint32ToSmi(input) ==> MLR: __ SmiTag(input)
  // ECL: ChangeUint64ToSmi(input) ==> MLR: __ SmiTag(input)

 private:
  // TODO(nicohartmann@): Might move some of those helpers into the assembler
  // interface.
  template <typename Rep = Any>
  V<Rep> LoadField(V<Tagged> object, const FieldAccess& access) {
    MachineType machine_type = access.machine_type;
    if (machine_type.IsMapWord()) {
      machine_type = MachineType::TaggedPointer();
#ifdef V8_MAP_PACKING
      UNIMPLEMENTED();
#endif
    }
    MemoryRepresentation rep =
        MemoryRepresentation::FromMachineType(machine_type);
#ifdef V8_ENABLE_SANDBOX
    bool is_sandboxed_external =
        access.type.Is(compiler::Type::ExternalPointer());
    if (is_sandboxed_external) {
      // Fields for sandboxed external pointer contain a 32-bit handle, not a
      // 64-bit raw pointer.
      rep = MemoryRepresentation::Uint32();
    }
#endif  // V8_ENABLE_SANDBOX
    V<Rep> value = __ Load(object, LoadOp::Kind::Aligned(access.base_is_tagged),
                           rep, access.offset);
#ifdef V8_ENABLE_SANDBOX
    if (is_sandboxed_external) {
      value = __ DecodeExternalPointer(value, access.external_pointer_tag);
    }
    if (access.is_bounded_size_access) {
      DCHECK(!is_sandboxed_external);
      value = __ ShiftRightLogical(value, kBoundedSizeShift,
                                   WordRepresentation::PointerSized());
    }
#endif  // V8_ENABLE_SANDBOX
    return value;
  }

  void StoreField(V<Tagged> object, const FieldAccess& access, V<Any> value) {
    // External pointer must never be stored by optimized code.
    DCHECK(!access.type.Is(compiler::Type::ExternalPointer()) ||
           !V8_ENABLE_SANDBOX_BOOL);
    // SandboxedPointers are not currently stored by optimized code.
    DCHECK(!access.type.Is(compiler::Type::SandboxedPointer()));

#ifdef V8_ENABLE_SANDBOX
    if (access.is_bounded_size_access) {
      value = __ ShiftLeft(value, kBoundedSizeShift,
                           WordRepresentation::PointerSized());
    }
#endif  // V8_ENABLE_SANDBOX

    StoreOp::Kind kind = StoreOp::Kind::Aligned(access.base_is_tagged);
    MachineType machine_type = access.machine_type;
    if (machine_type.IsMapWord()) {
      machine_type = MachineType::TaggedPointer();
#ifdef V8_MAP_PACKING
      UNIMPLEMENTED();
#endif
    }
    MemoryRepresentation rep =
        MemoryRepresentation::FromMachineType(machine_type);
    __ Store(object, value, kind, rep, access.write_barrier_kind,
             access.offset);
  }

  template <typename Rep = Any>
  V<Rep> LoadElement(V<Tagged> object, const ElementAccess& access,
                     V<WordPtr> index) {
    LoadOp::Kind kind = LoadOp::Kind::Aligned(access.base_is_tagged);
    MemoryRepresentation rep =
        MemoryRepresentation::FromMachineType(access.machine_type);
    return __ Load(object, index, kind, rep, access.header_size,
                   rep.SizeInBytesLog2());
  }

  // Pass {bitfield} = {digit} = OpIndex::Invalid() to construct the canonical
  // 0n BigInt.
  V<Tagged> AllocateBigInt(V<Word32> bitfield, V<Word64> digit) {
    DCHECK(Is64());
    DCHECK_EQ(bitfield.valid(), digit.valid());
    static constexpr auto zero_bitfield =
        BigInt::SignBits::update(BigInt::LengthBits::encode(0), false);

    V<Tagged> map = __ HeapConstant(factory_->bigint_map());
    V<Tagged> bigint =
        __ Allocate(__ IntPtrConstant(BigInt::SizeFor(digit.valid() ? 1 : 0)),
                    AllocationType::kYoung, AllowLargeObjects::kFalse);
    StoreField(bigint, AccessBuilder::ForMap(), map);
    StoreField(bigint, AccessBuilder::ForBigIntBitfield(),
               bitfield.valid() ? bitfield : __ Word32Constant(zero_bitfield));

    // BigInts have no padding on 64 bit architectures with pointer compression.
    if (BigInt::HasOptionalPadding()) {
      StoreField(bigint, AccessBuilder::ForBigIntOptionalPadding(),
                 __ IntPtrConstant(0));
    }
    if (digit.valid()) {
      StoreField(bigint, AccessBuilder::ForBigIntLeastSignificantDigit64(),
                 digit);
    }
    return bigint;
  }

  // TODO(nicohartmann@): Should also make this an operation and lower in
  // TagUntagLoweringReducer.
  V<Word32> IsSmi(V<Tagged> input) {
    return __ Word32Equal(
        __ Word32BitwiseAnd(V<Word32>::Cast(input),
                            static_cast<uint32_t>(kSmiTagMask)),
        static_cast<uint32_t>(kSmiTag));
  }

  void SmiTagOrOverflow(V<Word32> input, Label<>* overflow,
                        Label<Tagged>* done) {
    DCHECK(SmiValuesAre31Bits());

    // Check for overflow at the same time that we are smi tagging.
    // Since smi tagging shifts left by one, it's the same as adding value
    // twice.
    OpIndex add = __ Int32AddCheckOverflow(input, input);
    V<Word32> check = __ Projection(add, 1, WordRepresentation::Word32());
    GOTO_IF(check, *overflow);
    GOTO(*done, __ SmiTag(input));
  }

  V<Tagged> AllocateHeapNumberWithValue(V<Float64> value) {
    V<Tagged> result =
        __ Allocate(__ IntPtrConstant(HeapNumber::kSize),
                    AllocationType::kYoung, AllowLargeObjects::kFalse);
    StoreField(result, AccessBuilder::ForMap(),
               __ HeapConstant(factory_->heap_number_map()));
    StoreField(result, AccessBuilder::ForHeapNumberValue(), value);
    return result;
  }

  Factory* factory_;
};

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_MACHINE_LOWERING_REDUCER_H_
