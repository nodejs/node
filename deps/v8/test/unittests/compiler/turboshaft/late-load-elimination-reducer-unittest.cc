// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/late-load-elimination-reducer.h"

#include <cstddef>

#if V8_ENABLE_WEBASSEMBLY
#include "src/compiler/turboshaft/wasm-load-elimination-reducer.h"
#include "src/wasm/wasm-objects.h"
#endif

#include "src/codegen/interface-descriptors.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/globals.h"
#include "src/compiler/linkage.h"
#include "src/compiler/operator.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/builtin-call-descriptors.h"
#include "src/compiler/turboshaft/copying-phase.h"
#include "src/compiler/turboshaft/dead-code-elimination-reducer.h"
#include "src/compiler/turboshaft/index.h"
#include "src/compiler/turboshaft/machine-optimization-reducer.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/opmasks.h"
#include "src/compiler/turboshaft/phase.h"
#include "src/compiler/turboshaft/representations.h"
#include "src/compiler/turboshaft/required-optimization-reducer.h"
#include "src/compiler/turboshaft/runtime-call-descriptors.h"
#include "src/compiler/turboshaft/variable-reducer.h"
#include "src/flags/flags.h"
#include "src/objects/code.h"
#include "src/objects/heap-object.h"
#include "src/objects/string.h"
#include "test/common/flag-utils.h"
#include "test/unittests/compiler/js-heap-broker-base.h"
#include "test/unittests/compiler/turboshaft/reducer-test.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

#ifdef DEBUG
#define LATE_LOAD_ELIM_VERIFY v8_flags.turboshaft_verify_load_elimination
#else
#define LATE_LOAD_ELIM_VERIFY false
#endif

// Use like this:
// V<...> C(my_var) = ...
#define C(value) value = Asm.CaptureHelperForMacro(#value)

class LateLoadEliminationReducerTest : public ReducerTest {
 public:
  LateLoadEliminationReducerTest()
      : ReducerTest(),
        flag_load_elimination_(&v8_flags.turboshaft_load_elimination, true) {}

  void StoreToObject(
      TestInstance& Asm, V<HeapObject> object, V<WordPtr> offset, V<Any> value,
      MemoryRepresentation memory_rep,
      WriteBarrierKind write_barrier_kind = WriteBarrierKind::kNoWriteBarrier,
      bool initializing_transitioning = false) {
    __ Store(object, offset, value, StoreOp::Kind::TaggedBase(), memory_rep,
             write_barrier_kind, kHeapObjectTag, initializing_transitioning);
  }

  template <typename T = Any>
  V<T> LoadFromObject(TestInstance& Asm, V<HeapObject> object,
                      V<WordPtr> offset, MemoryRepresentation memory_rep) {
    return __ Load(object, offset, LoadOp::Kind::TaggedBase(), memory_rep,
                   kHeapObjectTag);
  }

  template <typename T>
  TestInstance CreateSimpleStoreLoadTest(MemoryRepresentation store_rep,
                                         MemoryRepresentation load_rep) {
    std::initializer_list<RegisterRepresentation> parameter_types{
        RegisterRepresentation::Tagged(), v_traits<T>::rep};
    return CreateFromGraph(base::VectorOf(parameter_types), [&](auto& Asm) {
      V<HeapObject> object = V<HeapObject>::Cast(Asm.GetParameter(0));
      V<WordPtr> offset = __ WordPtrConstant(5);
      V<T> C(value) = Asm.template GetParameter<T>(1);

      StoreToObject(Asm, object, offset, value, store_rep);
      V<Word32> C(load) = LoadFromObject<Word32>(Asm, object, offset, load_rep);

      __ Return(load);
    });
  }

 private:
  const FlagScope<bool> flag_load_elimination_;
};

TEST_F(LateLoadEliminationReducerTest, Store_Int32_Load_Int32) {
  auto test = CreateSimpleStoreLoadTest<Word32>(MemoryRepresentation::Int32(),
                                                MemoryRepresentation::Int32());

  test.Run<LateLoadEliminationReducer>();

  const ReturnOp* ret = test.graph()
                            .Get(test.graph().LastOperation())
                            .template TryCast<ReturnOp>();
  ASSERT_NE(nullptr, ret);
  ASSERT_EQ(1U, ret->return_values().size());
  OpIndex ret_val = ret->return_values()[0];

  ASSERT_TRUE(test.GetCapture("value").Is(ret_val));
  ASSERT_TRUE(test.GetCapture("load").IsEmpty());
}

TEST_F(LateLoadEliminationReducerTest, Store_Int64_Load_Int64) {
  auto test = CreateSimpleStoreLoadTest<Word64>(MemoryRepresentation::Int64(),
                                                MemoryRepresentation::Int64());

  test.Run<LateLoadEliminationReducer>();

  const ReturnOp* ret = test.graph()
                            .Get(test.graph().LastOperation())
                            .template TryCast<ReturnOp>();
  ASSERT_NE(nullptr, ret);
  ASSERT_EQ(1U, ret->return_values().size());
  OpIndex ret_val = ret->return_values()[0];

  ASSERT_TRUE(test.GetCapture("value").Is(ret_val));
  ASSERT_TRUE(test.GetCapture("load").IsEmpty());
}

// TODO(nicohartmann): This needs to be supported by LLE.
#if 0
TEST_F(LateLoadEliminationReducerTest, Store_Int64_Load_Int32) {
  auto test = CreateSimpleStoreLoadTest<Word64>(MemoryRepresentation::Int64(),
                                                MemoryRepresentation::Int32());

  test.Run<LateLoadEliminationReducer>();

  const ReturnOp* ret = test.graph()
                            .Get(test.graph().LastOperation())
                            .template TryCast<ReturnOp>();
  ASSERT_NE(nullptr, ret);
  ASSERT_EQ(1U, ret->return_values().size());

  const Operation& ret_val_op = test.graph().Get(ret->return_values()[0]);
  ASSERT_TRUE(ret_val_op.Is<Opmask::kTruncateWord64ToWord32>());

  ASSERT_TRUE(test.GetCapture("value").Is(ret_val_op.input(0)));
  ASSERT_TRUE(test.GetCapture("load").IsEmpty());
}

TEST_F(LateLoadEliminationReducerTest, Store_Int16_Load_Int16) {
  auto test = CreateSimpleStoreLoadTest<Word32>(MemoryRepresentation::Int16(),
                                                MemoryRepresentation::Int16());

  test.Run<LateLoadEliminationReducer>();

  const ReturnOp* ret = test.graph()
                            .Get(test.graph().LastOperation())
                            .template TryCast<ReturnOp>();
  ASSERT_NE(nullptr, ret);
  ASSERT_EQ(1U, ret->return_values().size());

  const Operation& ret_val_op = test.graph().Get(ret->return_values()[0]);
  ASSERT_TRUE(ret_val_op.Is<Opmask::kWord32ShiftRightArithmetic>());

  ASSERT_TRUE(test.GetCapture("value").Is(ret_val_op.input(0)));
  ASSERT_TRUE(test.GetCapture("load").IsEmpty());
}

TEST_F(LateLoadEliminationReducerTest, Store_Int16_Load_Uint8) {
  auto test = CreateSimpleStoreLoadTest<Word32>(MemoryRepresentation::Int16(),
                                                MemoryRepresentation::Uint8());

  test.Run<LateLoadEliminationReducer>();

  const ReturnOp* ret = test.graph()
                            .Get(test.graph().LastOperation())
                            .template TryCast<ReturnOp>();
  ASSERT_NE(nullptr, ret);
  ASSERT_EQ(1U, ret->return_values().size());

  const Operation& ret_val_op = test.graph().Get(ret->return_values()[0]);
  ASSERT_TRUE(ret_val_op.Is<Opmask::kWord32BitwiseAnd>());

  ASSERT_TRUE(test.GetCapture("value").Is(ret_val_op.input(0)));
  ASSERT_TRUE(test.GetCapture("load").IsEmpty());
}
#endif

/* TruncateInt64ToInt32(
 *     BitcastTaggedToWordPtrForTagAndSmiBits(
 *         Load[Tagged]))
 * => Load[Int32]
 */
TEST_F(LateLoadEliminationReducerTest, Int32TruncatedLoad_Foldable) {
  auto test = CreateFromGraph(2, [](auto& Asm) {
    V<Object> C(load) = __ Load(
        Asm.GetParameter(0), {}, LoadOp::Kind::TaggedBase(),
        MemoryRepresentation::AnyTagged(), RegisterRepresentation::Tagged(), 0);
    V<WordPtr> temp = __ BitcastTaggedToWordPtrForTagAndSmiBits(load);
    V<Word32> C(truncate) = __ TruncateWordPtrToWord32(temp);
    V<Object> C(result) =
        __ Conditional(truncate, Asm.GetParameter(0), Asm.GetParameter(1));
    __ Return(result);
  });

  test.Run<LateLoadEliminationReducer>();

#ifdef V8_COMPRESS_POINTERS

  // Load should have been replaced by an int32 load.
  const LoadOp* load = test.GetCapturedAs<LoadOp>("load");
  ASSERT_NE(load, nullptr);
  ASSERT_EQ(load->loaded_rep, MemoryRepresentation::Int32());
  ASSERT_EQ(load->result_rep, RegisterRepresentation::Word32());

  // The truncation chain should have been eliminated.
  ASSERT_TRUE(test.GetCapture("truncate").IsEmpty());

  // The select uses the load as condition directly.
  const SelectOp* result = test.GetCapturedAs<SelectOp>("result");
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(&test.graph().Get(result->cond()), load);

#endif
}

/* TruncateInt64ToInt32(
 *     BitcastTaggedToWordPtrForTagAndSmiBits(
 *         Load[Tagged]))
 * cannot be optimized because Load[Tagged] has another non-truncating use.
 */
TEST_F(LateLoadEliminationReducerTest,
       Int32TruncatedLoad_NonFoldable_AdditionalUse) {
  auto test = CreateFromGraph(1, [](auto& Asm) {
    V<Object> C(load) = __ Load(
        Asm.GetParameter(0), {}, LoadOp::Kind::TaggedBase(),
        MemoryRepresentation::AnyTagged(), RegisterRepresentation::Tagged(), 0);
    V<WordPtr> temp = __ BitcastTaggedToWordPtrForTagAndSmiBits(load);
    V<Word32> C(truncate) = __ TruncateWordPtrToWord32(temp);
    __ Return(__ Conditional(truncate, Asm.GetParameter(0), load));
  });

  test.Run<LateLoadEliminationReducer>();

#ifdef V8_COMPRESS_POINTERS

  // Load should still be tagged.
  const LoadOp* load = test.GetCapturedAs<LoadOp>("load");
  ASSERT_NE(load, nullptr);
  ASSERT_EQ(load->loaded_rep, MemoryRepresentation::AnyTagged());
  ASSERT_EQ(load->result_rep, RegisterRepresentation::Tagged());

  // The truncation chain should still be present.
  ASSERT_FALSE(test.GetCapture("truncate").IsEmpty());

#endif
}

/* TruncateInt64ToInt32(
 *     BitcastTaggedToWordPtrForTagAndSmiBits(
 *         Load[Tagged]))
 * cannot be optimized because there is another non-truncated Load that is
 * elminated by LateLoadElimination that adds additional uses.
 */
TEST_F(LateLoadEliminationReducerTest,
       Int32TruncatedLoad_NonFoldable_ReplacingOtherLoad) {
  auto test = CreateFromGraph(1, [](auto& Asm) {
    V<Object> C(load) = __ Load(
        Asm.GetParameter(0), {}, LoadOp::Kind::TaggedBase(),
        MemoryRepresentation::AnyTagged(), RegisterRepresentation::Tagged(), 0);
    V<WordPtr> temp = __ BitcastTaggedToWordPtrForTagAndSmiBits(load);
    V<Word32> C(truncate) = __ TruncateWordPtrToWord32(temp);
    V<Object> C(other_load) = __ Load(
        Asm.GetParameter(0), {}, LoadOp::Kind::TaggedBase(),
        MemoryRepresentation::AnyTagged(), RegisterRepresentation::Tagged(), 0);
    V<Object> C(result) =
        __ Conditional(truncate, Asm.GetParameter(0), other_load);
    __ Return(result);
  });

  test.Run<LateLoadEliminationReducer>();

#ifdef V8_COMPRESS_POINTERS

  // Load should still be tagged.
  const LoadOp* load = test.GetCapturedAs<LoadOp>("load");
  ASSERT_NE(load, nullptr);
  ASSERT_EQ(load->loaded_rep, MemoryRepresentation::AnyTagged());
  ASSERT_EQ(load->result_rep, RegisterRepresentation::Tagged());

  // The truncation chain should still be present.
  ASSERT_FALSE(test.GetCapture("truncate").IsEmpty());

  // The other load has been eliminated.
  if (!LATE_LOAD_ELIM_VERIFY) {
    ASSERT_TRUE(test.GetCapture("other_load").IsEmpty());
  }

  // The select's input is the first load.
  const SelectOp* result = test.GetCapturedAs<SelectOp>("result");
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(&test.graph().Get(result->vfalse()), load);

#endif
}

/* TruncateInt64ToInt32(
 *     BitcastTaggedToWordPtrForTagAndSmiBits(
 *         Load[Tagged]))
 * => Load[Int32]
 * because the other load that is eliminated by LateLoadElimination is also a
 * truncating load.
 */
TEST_F(LateLoadEliminationReducerTest,
       Int32TruncatedLoad_Foldable_ReplacingOtherLoad) {
  auto test = CreateFromGraph(1, [](auto& Asm) {
    V<Object> C(load) = __ Load(
        Asm.GetParameter(0), {}, LoadOp::Kind::TaggedBase(),
        MemoryRepresentation::AnyTagged(), RegisterRepresentation::Tagged(), 0);
    V<WordPtr> temp = __ BitcastTaggedToWordPtrForTagAndSmiBits(load);
    V<Word32> C(truncate) = __ TruncateWordPtrToWord32(temp);
    V<Object> C(other_load) = __ Load(
        Asm.GetParameter(0), {}, LoadOp::Kind::TaggedBase(),
        MemoryRepresentation::AnyTagged(), RegisterRepresentation::Tagged(), 0);
    V<WordPtr> other_temp =
        __ BitcastTaggedToWordPtrForTagAndSmiBits(other_load);
    V<Word32> C(other_truncate) = __ TruncateWordPtrToWord32(other_temp);
    V<Word32> C(result) =
        __ Conditional(truncate, __ Word32Constant(42), other_truncate);
    __ Return(__ TagSmi(result));
  });

  test.Run<LateLoadEliminationReducer>();

#ifdef V8_COMPRESS_POINTERS
  // Load should have been replaced by an int32 load.
  const LoadOp* load = test.GetCapturedAs<LoadOp>("load");
  ASSERT_NE(load, nullptr);
  ASSERT_EQ(load->loaded_rep, MemoryRepresentation::Int32());
  ASSERT_EQ(load->result_rep, RegisterRepresentation::Word32());

  // Both truncation chains should have been eliminated.
  ASSERT_TRUE(test.GetCapture("truncate").IsEmpty());
  ASSERT_TRUE(test.GetCapture("other_truncate").IsEmpty());

  // The other load should have been eliminated.
  if (!LATE_LOAD_ELIM_VERIFY) {
    ASSERT_TRUE(test.GetCapture("other_load").IsEmpty());
  }

  // The select uses the load as condition and the second input directly.
  const SelectOp* result = test.GetCapturedAs<SelectOp>("result");
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(&test.graph().Get(result->cond()), load);
  ASSERT_EQ(&test.graph().Get(result->vfalse()), load);

#endif
}

/* TruncateInt64ToInt32(
 *     BitcastTaggedToWordPtrForTagAndSmiBits(
 *         Load[Tagged]))
 * cannot be optimized because this load is replaced by another load that has
 * non-truncated uses.
 */
TEST_F(LateLoadEliminationReducerTest,
       Int32TruncatedLoad_NonFoldable_ReplacedByOtherLoad) {
  auto test = CreateFromGraph(1, [](auto& Asm) {
    V<Object> C(other_load) = __ Load(
        Asm.GetParameter(0), {}, LoadOp::Kind::TaggedBase(),
        MemoryRepresentation::AnyTagged(), RegisterRepresentation::Tagged(), 0);
    V<Object> C(load) = __ Load(
        Asm.GetParameter(0), {}, LoadOp::Kind::TaggedBase(),
        MemoryRepresentation::AnyTagged(), RegisterRepresentation::Tagged(), 0);
    V<WordPtr> temp = __ BitcastTaggedToWordPtrForTagAndSmiBits(load);
    V<Word32> C(truncate) = __ TruncateWordPtrToWord32(temp);
    V<Object> C(result) =
        __ Conditional(truncate, Asm.GetParameter(0), other_load);
    __ Return(result);
  });

  test.Run<LateLoadEliminationReducer>();

#ifdef V8_COMPRESS_POINTERS

  // The other load should still be tagged.
  const LoadOp* other_load = test.GetCapturedAs<LoadOp>("other_load");
  ASSERT_NE(other_load, nullptr);
  ASSERT_EQ(other_load->loaded_rep, MemoryRepresentation::AnyTagged());
  ASSERT_EQ(other_load->result_rep, RegisterRepresentation::Tagged());

  // The truncation chain should still be present.
  const ChangeOp* truncate = test.GetCapturedAs<ChangeOp>("truncate");
  ASSERT_NE(truncate, nullptr);
  // ... but the input is now the other load.
  const TaggedBitcastOp& bitcast =
      test.graph().Get(truncate->input()).Cast<TaggedBitcastOp>();
  ASSERT_EQ(other_load, &test.graph().Get(bitcast.input()));

  // The load has been eliminated.
  if (!LATE_LOAD_ELIM_VERIFY) {
    ASSERT_TRUE(test.GetCapture("load").IsEmpty());
  }

  // The select's input is unchanged.
  const SelectOp* result = test.GetCapturedAs<SelectOp>("result");
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(&test.graph().Get(result->cond()), truncate);
  ASSERT_EQ(&test.graph().Get(result->vfalse()), other_load);

#endif
}

/* TruncateInt64ToInt32(
 *     BitcastTaggedToWordPtrForTagAndSmiBits(
 *         Load[Tagged]))
 * => Load[Int32]
 * because the other load that is replacing the load is also a truncating load.
 */
TEST_F(LateLoadEliminationReducerTest,
       Int32TruncatedLoad_Foldable_ReplacedByOtherLoad) {
  auto test = CreateFromGraph(1, [](auto& Asm) {
    V<Object> C(other_load) = __ Load(
        Asm.GetParameter(0), {}, LoadOp::Kind::TaggedBase(),
        MemoryRepresentation::AnyTagged(), RegisterRepresentation::Tagged(), 0);
    V<Object> C(load) = __ Load(
        Asm.GetParameter(0), {}, LoadOp::Kind::TaggedBase(),
        MemoryRepresentation::AnyTagged(), RegisterRepresentation::Tagged(), 0);
    V<WordPtr> temp = __ BitcastTaggedToWordPtrForTagAndSmiBits(load);
    V<Word32> C(truncate) = __ TruncateWordPtrToWord32(temp);
    V<WordPtr> other_temp =
        __ BitcastTaggedToWordPtrForTagAndSmiBits(other_load);
    V<Word32> C(other_truncate) = __ TruncateWordPtrToWord32(other_temp);
    V<Word32> C(result) =
        __ Conditional(truncate, __ Word32Constant(42), other_truncate);
    __ Return(__ TagSmi(result));
  });

  test.Run<LateLoadEliminationReducer>();

#if V8_COMPRESS_POINTERS

  // The other load should be replaced by an int32 load.
  const LoadOp* other_load = test.GetCapturedAs<LoadOp>("other_load");
  ASSERT_NE(other_load, nullptr);
  ASSERT_EQ(other_load->loaded_rep, MemoryRepresentation::Int32());
  ASSERT_EQ(other_load->result_rep, RegisterRepresentation::Word32());

  // The truncation chains should be eliminated.
  ASSERT_TRUE(test.GetCapture("truncate").IsEmpty());
  ASSERT_TRUE(test.GetCapture("other_truncate").IsEmpty());

  // The load has been eliminated.
  if (!LATE_LOAD_ELIM_VERIFY) {
    ASSERT_TRUE(test.GetCapture("load").IsEmpty());
  }

  // The select uses the other load as condition and the second input directly.
  const SelectOp* result = test.GetCapturedAs<SelectOp>("result");
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(&test.graph().Get(result->cond()), other_load);
  ASSERT_EQ(&test.graph().Get(result->vfalse()), other_load);

#endif
}

/* TruncateInt64ToInt32(
 *     BitcastTaggedToWordPtrForTagAndSmiBits(
 *         Load[Tagged]))
 * cannot be optimized because the BitcastTaggedToWordPtrForTagAndSmiBits has an
 * additional (potentially non-truncating) use.
 */
TEST_F(LateLoadEliminationReducerTest,
       Int32TruncatedLoad_NonFoldable_AdditionalBitcastUse) {
  auto test = CreateFromGraph(1, [](auto& Asm) {
    V<Object> C(load) = __ Load(
        Asm.GetParameter(0), {}, LoadOp::Kind::TaggedBase(),
        MemoryRepresentation::AnyTagged(), RegisterRepresentation::Tagged(), 0);
    V<WordPtr> temp = __ BitcastTaggedToWordPtrForTagAndSmiBits(load);
    V<Word32> C(truncate) = __ TruncateWordPtrToWord32(temp);
    V<WordPtr> C(result) = __ Conditional(
        truncate, __ BitcastTaggedToWordPtr(Asm.GetParameter(0)), temp);
    __ Return(__ BitcastWordPtrToSmi(result));
  });

  test.Run<LateLoadEliminationReducer>();

#ifdef V8_COMPRESS_POINTERS

  // The load should still be tagged.
  const LoadOp* other_load = test.GetCapturedAs<LoadOp>("load");
  ASSERT_NE(other_load, nullptr);
  ASSERT_EQ(other_load->loaded_rep, MemoryRepresentation::AnyTagged());
  ASSERT_EQ(other_load->result_rep, RegisterRepresentation::Tagged());

  // The truncation chain should still be present.
  const ChangeOp* truncate = test.GetCapturedAs<ChangeOp>("truncate");
  ASSERT_NE(truncate, nullptr);

  // The select's input is unchanged.
  const SelectOp* result = test.GetCapturedAs<SelectOp>("result");
  ASSERT_NE(result, nullptr);
  ASSERT_EQ(&test.graph().Get(result->cond()), truncate);

#endif
}

TEST_F(LateLoadEliminationReducerTest,
       IndexedStore_Invalidates_Offset_On_Other_Object) {
  auto test = CreateFromGraph(
      base::VectorOf({RegisterRepresentation::Tagged(),
                      RegisterRepresentation::Tagged(),
                      RegisterRepresentation::WordPtr()}),
      [](auto& Asm) {
        V<HeapObject> obj1 = V<HeapObject>::Cast(Asm.GetParameter(0));
        V<HeapObject> obj2 = V<HeapObject>::Cast(Asm.GetParameter(1));
        V<WordPtr> index = Asm.template GetParameter<WordPtr>(2);

        // Initial load from obj2 at offset 8.
        constexpr int kOffset = 8;
        constexpr MemoryRepresentation kRep = MemoryRepresentation::Int32();
        V<Word32> C(load0) = __ Load(obj2, {}, LoadOp::Kind::TaggedBase(), kRep,
                                     RegisterRepresentation::Word32(), kOffset);

        // Indexed store to obj1 (which may alias obj2).
        __ Store(obj1, index, __ Word32Constant(42),
                 StoreOp::Kind::TaggedBase(), kRep,
                 WriteBarrierKind::kNoWriteBarrier, /* offset= */ 0,
                 kRep.SizeInBytesLog2());

        // Second load from obj2 at offset 8.
        V<Word32> C(load1) = __ Load(obj2, {}, LoadOp::Kind::TaggedBase(), kRep,
                                     RegisterRepresentation::Word32(), kOffset);

        __ Return(__ Word32Constant(0), base::VectorOf({load0, load1}));
      });

  test.Run<LateLoadEliminationReducer>();

  // load1 MUST NOT be eliminated because obj1 may alias obj2 at runtime!
  const LoadOp* load1 = test.GetCapturedAs<LoadOp>("load1");
  ASSERT_NE(load1, nullptr);
}

TEST_F(LateLoadEliminationReducerTest,
       FindOrderedHashEntry_Invalidates_String_First_Load) {
  {
    auto effects_test = CreateFromGraph(2, [](auto& Asm) {
      V<Object> table = Asm.GetParameter(0);
      V<Smi> key = V<Smi>::Cast(Asm.GetParameter(1));
      V<Smi> C(map_op) = __ FindOrderedHashMapEntry(table, key);
      V<Smi> C(set_op) = __ FindOrderedHashSetEntry(table, key);
      V<WordPtr> C(int32_op) =
          __ FindOrderedHashMapEntryForInt32Key(table, __ Word32Constant(0));
      __ Return(__ Word32Constant(0),
                base::VectorOf<OpIndex>({map_op, set_op, int32_op}));
    });
    const Operation* map_op = effects_test.GetCapture("map_op").Get();
    EXPECT_TRUE(map_op->Effects().can_write());
    EXPECT_TRUE(map_op->Effects().can_allocate);
    EXPECT_FALSE(map_op->IsRequiredWhenUnused());

    const Operation* set_op = effects_test.GetCapture("set_op").Get();
    EXPECT_TRUE(set_op->Effects().can_write());
    EXPECT_TRUE(set_op->Effects().can_allocate);
    EXPECT_FALSE(set_op->IsRequiredWhenUnused());

    const Operation* int32_op = effects_test.GetCapture("int32_op").Get();
    EXPECT_FALSE(int32_op->Effects().can_write());
    EXPECT_FALSE(int32_op->Effects().can_allocate);
    EXPECT_TRUE(int32_op->Effects().produces.load_heap_memory);
  }

  EXPECT_EQ(AccessBuilder::ForConsStringFirst().offset,
            AccessBuilder::ForThinStringActual().offset);

  auto test = CreateFromGraph(3, [](auto& Asm) {
    V<Object> table = Asm.GetParameter(0);
    V<String> first0 = V<String>::Cast(Asm.GetParameter(1));
    V<String> first1 = V<String>::Cast(Asm.GetParameter(2));

    auto escaped_alloc = __ template Allocate<ConsString>(
        sizeof(ConsString), AllocationType::kYoung,
        AllocationAlignment::kTaggedAligned);
    __ InitializeField(escaped_alloc, AccessBuilder::ForConsStringFirst(),
                       first0);
    V<ConsString> escaped_str =
        __ FinishInitialization(std::move(escaped_alloc));

    auto local_alloc = __ template Allocate<ConsString>(
        sizeof(ConsString), AllocationType::kYoung,
        AllocationAlignment::kTaggedAligned);
    __ InitializeField(local_alloc, AccessBuilder::ForConsStringFirst(),
                       first1);
    V<ConsString> local_str = __ FinishInitialization(std::move(local_alloc));

    V<Smi> entry = __ template CallBuiltin<builtin::FindOrderedHashMapEntry>(
        Asm.context(), {.table = table, .key = escaped_str});

    // String::MakeThin overwrites ConsString::first_ with ThinString::actual_.
    V<String> C(escaped_load) = __ template LoadField<String>(
        escaped_str, AccessBuilder::ForThinStringActual());
    V<String> C(local_load) = __ template LoadField<String>(
        local_str, AccessBuilder::ForConsStringFirst());

    __ Return(__ Word32Constant(0),
              base::VectorOf<OpIndex>({entry, escaped_load, local_load}));
  });

  test.Run<LateLoadEliminationReducer>();

  // The load from the escaped ConsString must NOT be replaced with `first0`.
  const LoadOp* escaped_load = test.GetCapturedAs<LoadOp>("escaped_load");
  ASSERT_NE(escaped_load, nullptr);

  // Non-escaping allocations remain eligible for load elimination.
  if (!LATE_LOAD_ELIM_VERIFY) {
    ASSERT_TRUE(test.GetCapture("local_load").IsEmpty());
  }

  {
    TestInstance dce_test = CreateFromGraph(
        base::VectorOf({RegisterRepresentation::Tagged(),
                        RegisterRepresentation::Tagged(),
                        RegisterRepresentation::Word32()}),
        [](TestInstance& Asm) {
          V<Object> table = Asm.GetParameter<Object>(0);
          V<Smi> key = Asm.GetParameter<Smi>(1);
          V<Word32> cond = Asm.GetParameter<Word32>(2);
          IF (cond) {
            V<Smi> C(unused_map_op) = __ FindOrderedHashMapEntry(table, key);
            V<Smi> C(unused_set_op) = __ FindOrderedHashSetEntry(table, key);
            V<Smi> C(unused_builtin_call) =
                __ CallBuiltin<builtin::FindOrderedHashMapEntry>(
                    Asm.context(), {.table = table, .key = key});
            USE(unused_map_op);
            USE(unused_set_op);
            USE(unused_builtin_call);
          }
          __ Return(__ Word32Constant(0));
        });

    dce_test.Run<DeadCodeEliminationReducer>();

    // Unused ordered hash lookups have no observable side effects and are
    // eliminatable.
    EXPECT_TRUE(dce_test.GetCapture("unused_map_op").IsEmpty());
    EXPECT_TRUE(dce_test.GetCapture("unused_set_op").IsEmpty());
    EXPECT_TRUE(dce_test.GetCapture("unused_builtin_call").IsEmpty());
    EXPECT_LE(dce_test.graph().block_count(), 2u);
  }
}

TEST_F(LateLoadEliminationReducerTest,
       CatchBlockBeginPreservesNonStringMapsAndLoads) {
  JSHeapBrokerTestBase broker_base(isolate(), zone());
  MapRef map1 = MakeRef(
      broker_base.broker(),
      broker_base.CanonicalHandle(isolate()->factory()->heap_number_map()));
  MapRef map2 = MakeRef(
      broker_base.broker(),
      broker_base.CanonicalHandle(isolate()->factory()->fixed_array_map()));
  ZoneRefSet<Map> maps1(map1);
  ZoneRefSet<Map> maps2(map2);

  auto test = CreateFromGraph(4, [&](auto& Asm) {
    V<HeapObject> o1 = V<HeapObject>::Cast(Asm.GetParameter(0));
    V<HeapObject> o2 = V<HeapObject>::Cast(Asm.GetParameter(1));
    V<String> s1 = V<String>::Cast(Asm.GetParameter(2));
    V<String> s2 = V<String>::Cast(Asm.GetParameter(3));
    constexpr int kFieldOffset = 12;

    __ AssumeMap(o1, maps1);
    V<Any> load0 = __ Load(o1, LoadOp::Kind::TaggedBase(),
                           MemoryRepresentation::AnyTagged(), kFieldOffset);

    Block* catch_block = __ NewBlock();
    V<String> concat;
    {
      CatchScopeImpl<assembler_t> scope(Asm.Asm(), catch_block);
      concat = __ template CallBuiltin<builtin::StringAdd_NoMapCheck>(
          Asm.context(), {.left = s1, .right = s2});
    }
    __ Return(concat);

    if (__ Bind(catch_block)) {
      __ CatchBlockBegin();
      V<Any> C(load1) =
          __ Load(o1, LoadOp::Kind::TaggedBase(),
                  MemoryRepresentation::AnyTagged(), kFieldOffset);
      __ AssumeMap(o2, maps2);
      __ Store(o2, __ SmiConstant(Smi::FromInt(42)),
               StoreOp::Kind::TaggedBase(), MemoryRepresentation::AnyTagged(),
               WriteBarrierKind::kNoWriteBarrier, kFieldOffset);
      V<Any> C(load2) =
          __ Load(o1, LoadOp::Kind::TaggedBase(),
                  MemoryRepresentation::AnyTagged(), kFieldOffset);
      __ Return(__ Word32Constant(0), base::VectorOf({load0, load1, load2}));
    }
  });

  test.Run<LateLoadEliminationReducer>();

  if (!LATE_LOAD_ELIM_VERIFY) {
    ASSERT_TRUE(test.GetCapture("load1").IsEmpty());
    ASSERT_TRUE(test.GetCapture("load2").IsEmpty());
  }
}

TEST_F(LateLoadEliminationReducerTest, CatchBlockBeginInvalidatesStringMaps) {
  JSHeapBrokerTestBase broker_base(isolate(), zone());
  MapRef str_map =
      MakeRef(broker_base.broker(),
              broker_base.CanonicalHandle(
                  isolate()->factory()->seq_one_byte_string_map()));
  MapRef other_map = MakeRef(
      broker_base.broker(),
      broker_base.CanonicalHandle(isolate()->factory()->fixed_array_map()));
  ZoneRefSet<Map> str_maps(str_map);
  ZoneRefSet<Map> other_maps(other_map);

  const TSCallDescriptor* throw_desc = TSCallDescriptor::Create(
      Linkage::GetStubCallDescriptor(zone(), VoidDescriptor{}, 0,
                                     CallDescriptor::kNoFlags,
                                     Operator::kNoWrite),
      CanThrow{true}, LazyDeoptOnThrow{false}, zone());
  auto test = CreateFromGraph(3, [&](auto& Asm) {
    V<HeapObject> o1 = V<HeapObject>::Cast(Asm.GetParameter(0));
    V<HeapObject> o2 = V<HeapObject>::Cast(Asm.GetParameter(1));
    V<Code> callee = V<Code>::Cast(Asm.GetParameter(2));
    constexpr int kFieldOffset = 12;

    __ AssumeMap(o1, str_maps);
    V<Map> map0 = __ Load(o1, LoadOp::Kind::TaggedBase(),
                          MemoryRepresentation::TaggedPointer(),
                          offsetof(HeapObject, map_));
    V<Any> load0 = __ Load(o1, LoadOp::Kind::TaggedBase(),
                           MemoryRepresentation::AnyTagged(), kFieldOffset);

    Block* catch_block = __ NewBlock();
    OpIndex res;
    {
      CatchScopeImpl<assembler_t> scope(Asm.Asm(), catch_block);
      res = __ Call(callee, OptionalV<LazyFrameState>::Nullopt(),
                    base::VectorOf<OpIndex>({Asm.context()}), throw_desc,
                    OpEffects().CanDependOnChecks().CanLeaveCurrentFunction());
    }
    __ Return(__ Word32Constant(0),
              base::VectorOf<OpIndex>({map0, load0, res}));

    if (__ Bind(catch_block)) {
      __ CatchBlockBegin();
      V<Map> C(map1) = __ Load(o1, LoadOp::Kind::TaggedBase(),
                               MemoryRepresentation::TaggedPointer(),
                               offsetof(HeapObject, map_));
      __ AssumeMap(o2, other_maps);
      __ Store(o2, __ SmiConstant(Smi::FromInt(42)),
               StoreOp::Kind::TaggedBase(), MemoryRepresentation::AnyTagged(),
               WriteBarrierKind::kNoWriteBarrier, kFieldOffset);
      V<Any> C(load1) =
          __ Load(o1, LoadOp::Kind::TaggedBase(),
                  MemoryRepresentation::AnyTagged(), kFieldOffset);
      __ Return(__ Word32Constant(0),
                base::VectorOf<OpIndex>({map0, map1, load0, load1}));
    }
  });

  test.Run<LateLoadEliminationReducer>();

  // Exception handling (`CheckExceptionOp` / `CatchBlockBeginOp`) has
  // `can_allocate`, so both loaded string maps and assumed string maps must be
  // invalidated even when the throwing call does not declare `CanAllocate()`.
  ASSERT_NE(test.GetCapturedAs<LoadOp>("map1"), nullptr);
  ASSERT_NE(test.GetCapturedAs<LoadOp>("load1"), nullptr);
}

TEST_F(LateLoadEliminationReducerTest,
       CatchBlockBeginInvalidatesMemoryInBuiltins) {
  for (TurboshaftPipelineKind pipeline_kind : {
           TurboshaftPipelineKind::kCSA,
           TurboshaftPipelineKind::kTSABuiltin,
#if V8_ENABLE_WEBASSEMBLY
           TurboshaftPipelineKind::kJSToWasm,
#endif
       }) {
    auto test = CreateFromGraph(
        4,
        [&](auto& Asm) {
          V<HeapObject> o1 = Asm.template GetParameter<HeapObject>(0);
          V<String> s1 = Asm.template GetParameter<String>(1);
          V<String> s2 = Asm.template GetParameter<String>(2);
          V<String> init_val = Asm.template GetParameter<String>(3);
          constexpr int kFieldOffset = 12;

          V<Object> load0 =
              __ Load(o1, LoadOp::Kind::TaggedBase(),
                      MemoryRepresentation::AnyTagged(), kFieldOffset);

          auto local_alloc = __ template Allocate<ConsString>(
              sizeof(ConsString), AllocationType::kYoung,
              AllocationAlignment::kTaggedAligned);
          __ InitializeField(local_alloc, AccessBuilder::ForConsStringFirst(),
                             init_val);
          V<ConsString> local_str =
              __ FinishInitialization(std::move(local_alloc));

          Block* catch_block = __ NewBlock();
          V<String> concat;
          {
            CatchScopeImpl<assembler_t> scope(Asm.Asm(), catch_block);
            concat = __ template CallBuiltin<builtin::StringAdd_NoMapCheck>(
                Asm.context(), {.left = s1, .right = s2});
          }
          V<Object> C(load_fast) =
              __ Load(o1, LoadOp::Kind::TaggedBase(),
                      MemoryRepresentation::AnyTagged(), kFieldOffset);
          __ Return(__ Word32Constant(0),
                    base::VectorOf<OpIndex>({load0, concat, load_fast}));

          if (__ Bind(catch_block)) {
            __ CatchBlockBegin();
            V<Object> C(load_catch) =
                __ Load(o1, LoadOp::Kind::TaggedBase(),
                        MemoryRepresentation::AnyTagged(), kFieldOffset);
            V<String> C(local_load_catch) = __ template LoadField<String>(
                local_str, AccessBuilder::ForConsStringFirst());
            __ Return(
                __ Word32Constant(0),
                base::VectorOf<OpIndex>({load0, load_catch, local_load_catch}));
          }
        },
        pipeline_kind);

    test.Run<LateLoadEliminationReducer>();

    // Builtin and JS-to-Wasm wrapper frames cannot lazy-deoptimize on
    // `Debug::OnException`, so `CatchBlockBeginOp` must invalidate
    // maybe-aliasing memory in those pipelines without pessimizing the
    // non-throwing fast path or non-aliasing allocations.
    EXPECT_NE(test.GetCapturedAs<LoadOp>("load_catch"), nullptr);
    if (!LATE_LOAD_ELIM_VERIFY) {
      EXPECT_TRUE(test.GetCapture("load_fast").IsEmpty());
      EXPECT_TRUE(test.GetCapture("local_load_catch").IsEmpty());
    }
  }
}

TEST_F(LateLoadEliminationReducerTest,
       ImmutableSlotTrackingAcrossSnapshotsAndBranches) {
  auto test = CreateFromGraph(
      base::VectorOf({RegisterRepresentation::Tagged(),
                      RegisterRepresentation::Word32(),
                      RegisterRepresentation::Word32()}),
      [](auto& Asm) {
        V<HeapObject> obj = V<HeapObject>::Cast(Asm.GetParameter(0));
        V<Word32> val = Asm.template GetParameter<Word32>(1);
        V<Word32> cond = Asm.template GetParameter<Word32>(2);

        constexpr auto kInt32 = MemoryRepresentation::Int32();
        constexpr auto kMutL = LoadOp::Kind::TaggedBase();
        constexpr auto kImmL = kMutL.Immutable();
        auto StoreMut = [&](V<HeapObject> base, int off, V<Word32> v) {
          __ Store(base, v, StoreOp::Kind::TaggedBase(), kInt32,
                   WriteBarrierKind::kNoWriteBarrier, off);
        };
        auto WritingCall = [&]() {
          __ template CallBuiltin<builtin::CopyFastSmiOrObjectElements>(
              {.object = obj});
        };

        StoreMut(obj, 8, val);
        V<Word32> if_promote, if_after_call, else_after_call;
        IF (cond) {
          // Promotes `{val, false}` to `{val, true}`, surviving `WritingCall`.
          C(if_promote) = __ Load(obj, kImmL, kInt32, 8);
          WritingCall();
          C(if_after_call) = __ Load(obj, kMutL, kInt32, 8);
        } ELSE {
          // Promotion in `THEN` is rolled back, so `WritingCall` invalidates.
          WritingCall();
          C(else_after_call) = __ Load(obj, kMutL, kInt32, 8);
          StoreMut(obj, 8, val);
        }
        V<Word32> phi1 = __ Phi(base::VectorOf({if_promote, val}),
                                RegisterRepresentation::Word32());
        V<Word32> phi2 =
            __ Phi(base::VectorOf({if_after_call, else_after_call}),
                   RegisterRepresentation::Word32());

        // Merging `{val, true}` with `{val, false}` yields `{val, false}`:
        // valid before `WritingCall`, invalidated after.
        V<Word32> C(merged_before_call) = __ Load(obj, kMutL, kInt32, 8);
        WritingCall();
        V<Word32> C(merged_after_call) = __ Load(obj, kMutL, kInt32, 8);

        __ Return(__ Word32Constant(0),
                  base::VectorOf<OpIndex>(
                      {phi1, phi2, merged_before_call, merged_after_call}));
      });

  test.Run<LateLoadEliminationReducer>();

  EXPECT_NE(test.GetCapturedAs<LoadOp>("else_after_call"), nullptr);
  EXPECT_NE(test.GetCapturedAs<LoadOp>("merged_after_call"), nullptr);
  if (!LATE_LOAD_ELIM_VERIFY) {
    for (const char* name :
         {"if_promote", "if_after_call", "merged_before_call"}) {
      EXPECT_TRUE(test.GetCapture(name).IsEmpty()) << name;
    }
  }
}

#if V8_ENABLE_WEBASSEMBLY

class WasmLateLoadEliminationReducerTest
    : public LateLoadEliminationReducerTest {
 public:
  WasmLateLoadEliminationReducerTest()
      : LateLoadEliminationReducerTest(),
        flag_wasm_shared_(&v8_flags.wasm_shared, true) {}

 private:
  const FlagScope<bool> flag_wasm_shared_;
};

TEST_F(WasmLateLoadEliminationReducerTest, WasmCatchBlockBeginPreservesMemory) {
  auto test = CreateFromGraph(
      3,
      [](auto& Asm) {
        auto MakeLoad = [&Asm]() -> V<Word32> {
          return __ Load(Asm.GetParameter(0), {}, LoadOp::Kind::TaggedBase(),
                         MemoryRepresentation::Int32(),
                         RegisterRepresentation::Word32(), 4);
        };

        V<Word32> load0 = MakeLoad();

        Block* catch_block = __ NewBlock();
        V<String> concat;
        {
          CatchScopeImpl<assembler_t> scope(Asm.Asm(), catch_block);
          concat = __ template CallBuiltin<builtin::StringAdd_NoMapCheck>(
              Asm.context(), {.left = V<String>::Cast(Asm.GetParameter(1)),
                              .right = V<String>::Cast(Asm.GetParameter(2))});
        }
        __ Return(__ Word32Constant(0),
                  base::VectorOf<OpIndex>({load0, concat}));

        if (__ Bind(catch_block)) {
          __ CatchBlockBegin();
          V<Word32> C(load1) = MakeLoad();
          __ Return(__ Word32Constant(0), base::VectorOf({load0, load1}));
        }
      },
      /* is_wasm */ true);

  test.Run<LateLoadEliminationReducer>();

  if (!LATE_LOAD_ELIM_VERIFY) {
    ASSERT_TRUE(test.GetCapture("load1").IsEmpty());
  }
}

TEST_F(WasmLateLoadEliminationReducerTest, WasmTriviallyEliminatedLoad) {
  auto test = CreateFromGraph(
      1,
      [](auto& Asm) {
        auto MakeLoad = [&Asm]() -> V<Word32> {
          return __ Load(Asm.GetParameter(0), {}, LoadOp::Kind::TaggedBase(),
                         MemoryRepresentation::Int32(),
                         RegisterRepresentation::Word32(), 4);
        };

        V<Word32> load0 = MakeLoad();

        V<Word32> C(load1) = MakeLoad();

        // Make sure to keep both loads alive.
        __ Return(__ Word32Constant(0), base::VectorOf({load0, load1}));
      },
      /* is_wasm */ true);

  test.Run<LateLoadEliminationReducer>();

  // Baseline: load elimination occurs.
  ASSERT_TRUE(test.GetCapture("load1").IsEmpty());
}

TEST_F(WasmLateLoadEliminationReducerTest,
       WasmStringHashInvalidatesLoadAndIsEliminatable) {
  {
    TestInstance test = CreateFromGraph(
        1,
        [](TestInstance& Asm) {
          V<String> str = Asm.GetParameter<String>(0);
          V<Word32> load0 =
              __ LoadField<Word32>(str, AccessBuilder::ForNameRawHashField());
          V<Word32> C(hash_call) = __ WasmCallBuiltinThroughJumptable<
              deprecated::BuiltinCallDescriptor::WasmStringHash>({str});
          V<Word32> C(load1) =
              __ LoadField<Word32>(str, AccessBuilder::ForNameRawHashField());
          __ Return(__ Word32Constant(0),
                    base::VectorOf<OpIndex>({load0, hash_call, load1}));
        },
        /* is_wasm */ true);

    const Operation* hash_op = test.GetCapture("hash_call").Get();
    ASSERT_NE(hash_op, nullptr);
    EXPECT_TRUE(hash_op->Effects().can_write());
    EXPECT_FALSE(hash_op->IsRequiredWhenUnused());

    test.Run<LateLoadEliminationReducer>();

    // Live WasmStringHash writes Name::raw_hash_field_, so load1 must stay.
    const LoadOp* load1 = test.GetCapturedAs<LoadOp>("load1");
    ASSERT_NE(load1, nullptr);
  }

  {
    TestInstance dce_test = CreateFromGraph(
        base::VectorOf({RegisterRepresentation::Tagged(),
                        RegisterRepresentation::Word32()}),
        [](TestInstance& Asm) {
          V<String> str = Asm.GetParameter<String>(0);
          V<Word32> cond = Asm.GetParameter<Word32>(1);
          IF (cond) {
            V<Word32> C(unused_hash) = __ WasmCallBuiltinThroughJumptable<
                deprecated::BuiltinCallDescriptor::WasmStringHash>({str});
            USE(unused_hash);
          }
          __ Return(__ Word32Constant(0));
        },
        /* is_wasm */ true);

    dce_test.Run<DeadCodeEliminationReducer>();

    // Unused WasmStringHash has no observable side effects and is eliminatable.
    EXPECT_TRUE(dce_test.GetCapture("unused_hash").IsEmpty());
    EXPECT_LE(dce_test.graph().block_count(), 2u);
  }
}

TEST_F(WasmLateLoadEliminationReducerTest,
       WasmExternConvertAnyInvalidatesLoadAndIsEliminatable) {
  {
    TestInstance test = CreateFromGraph(1, [](TestInstance& Asm) {
      V<Object> obj = Asm.GetParameter<Object>(0);
      V<Object> load0 = __ Load(obj, LoadOp::Kind::TaggedBase(),
                                MemoryRepresentation::TaggedPointer(),
                                offsetof(WasmCustomMap, js_wrapper_));
      V<JSObject> C(convert_call) =
          __ CallRuntime<runtime::WasmExternConvertAny>(__ NoContextConstant(),
                                                        {.object = obj});
      V<Object> C(load1) = __ Load(obj, LoadOp::Kind::TaggedBase(),
                                   MemoryRepresentation::TaggedPointer(),
                                   offsetof(WasmCustomMap, js_wrapper_));
      __ Return(__ Word32Constant(0),
                base::VectorOf<OpIndex>({load0, convert_call, load1}));
    });

    const Operation* convert_op = test.GetCapture("convert_call").Get();
    ASSERT_NE(convert_op, nullptr);
    EXPECT_TRUE(convert_op->Effects().can_write());
    EXPECT_FALSE(convert_op->IsRequiredWhenUnused());

    test.Run<LateLoadEliminationReducer>();

    // Live WasmExternConvertAny writes WasmCustomMap::js_wrapper_, so load1
    // must stay.
    const LoadOp* load1 = test.GetCapturedAs<LoadOp>("load1");
    ASSERT_NE(load1, nullptr);
  }

  {
    TestInstance dce_test =
        CreateFromGraph(base::VectorOf({RegisterRepresentation::Tagged(),
                                        RegisterRepresentation::Word32()}),
                        [](TestInstance& Asm) {
                          V<Object> obj = Asm.GetParameter<Object>(0);
                          V<Word32> cond = Asm.GetParameter<Word32>(1);
                          IF (cond) {
                            V<JSObject> C(unused_convert) =
                                __ CallRuntime<runtime::WasmExternConvertAny>(
                                    __ NoContextConstant(), {.object = obj});
                            USE(unused_convert);
                          }
                          __ Return(__ Word32Constant(0));
                        });

    dce_test.Run<DeadCodeEliminationReducer>();

    // Unused WasmExternConvertAny has no observable side effects and is
    // eliminatable.
    EXPECT_TRUE(dce_test.GetCapture("unused_convert").IsEmpty());
    EXPECT_LE(dce_test.graph().block_count(), 2u);
  }
}

TEST_F(WasmLateLoadEliminationReducerTest, WasmAtomicsNoElimination) {
  auto test = CreateFromGraph(
      1,
      [](auto& Asm) {
        auto MakeLoad = [&Asm]() -> V<Word32> {
          return __ Load(
              Asm.GetParameter(0), {},
              LoadOp::Kind::TaggedBase().Atomic(AtomicMemoryOrder::kSeqCst),
              MemoryRepresentation::Int32(), RegisterRepresentation::Word32(),
              4);
        };

        V<Word32> load0 = MakeLoad();
        V<Word32> C(load1) = MakeLoad();
        __ Return(__ Word32Constant(0), base::VectorOf({load0, load1}));
      },
      /* is_wasm */ true);

  test.Run<LateLoadEliminationReducer>();

  const LoadOp* load1 = test.GetCapturedAs<LoadOp>("load1");
  // No load elimination for atomics.
  // TODO(manoskouk): Improve this.
  ASSERT_NE(load1, nullptr);
}

TEST_F(WasmLateLoadEliminationReducerTest, WasmInterveningNonAtomicStore) {
  auto test = CreateFromGraph(
      1,
      [](auto& Asm) {
        auto MakeLoad = [&Asm]() -> V<Word32> {
          return __ Load(Asm.GetParameter(0), {}, LoadOp::Kind::TaggedBase(),
                         MemoryRepresentation::Int32(),
                         RegisterRepresentation::Word32(), 4);
        };

        V<Word32> load0 = MakeLoad();

        V<HeapObject> fresh = __ FinishInitialization(
            __ Allocate(16, AllocationType::kSharedOld,
                        AllocationAlignment::kTaggedAligned));

        __ Store(fresh, {}, __ Word32Constant(1), StoreOp::Kind::TaggedBase(),
                 MemoryRepresentation::Int32(),
                 WriteBarrierKind::kNoWriteBarrier, 8);

        V<Word32> C(load1) = MakeLoad();

        __ Return(__ Word32Constant(0), base::VectorOf({load0, load1}));
      },
      /* is_wasm */ true);

  test.Run<LateLoadEliminationReducer>();

  // Baseline: load elimination happens if there exists an intervening store.
  ASSERT_TRUE(test.GetCapture("load1").IsEmpty());
}

TEST_F(WasmLateLoadEliminationReducerTest, WasmInterveningAtomicStore) {
  auto test = CreateFromGraph(
      1,
      [](auto& Asm) {
        auto MakeLoad = [&Asm]() -> V<Word32> {
          return __ Load(Asm.GetParameter(0), {}, LoadOp::Kind::TaggedBase(),
                         MemoryRepresentation::Int32(),
                         RegisterRepresentation::Word32(), 4);
        };

        V<Word32> load0 = MakeLoad();

        V<HeapObject> fresh = __ FinishInitialization(
            __ Allocate(16, AllocationType::kSharedOld,
                        AllocationAlignment::kTaggedAligned));

        __ Store(fresh, {}, __ Word32Constant(1),
                 StoreOp::Kind::TaggedBase().Atomic(AtomicMemoryOrder::kSeqCst),
                 MemoryRepresentation::Int32(),
                 WriteBarrierKind::kNoWriteBarrier, 8);

        V<Word32> C(load1) = MakeLoad();

        __ Return(__ Word32Constant(0), base::VectorOf({load0, load1}));
      },
      /* is_wasm */ true);

  test.Run<LateLoadEliminationReducer>();

  const LoadOp* load1 = test.GetCapturedAs<LoadOp>("load1");

  // No load elimination if there exists an intervening atomic store.
  ASSERT_NE(load1, nullptr);
}

TEST_F(WasmLateLoadEliminationReducerTest, WasmInterveningAtomicLoad) {
  auto test = CreateFromGraph(
      1,
      [](auto& Asm) {
        auto MakeLoad = [&Asm]() -> V<Word32> {
          return __ Load(Asm.GetParameter(0), {}, LoadOp::Kind::TaggedBase(),
                         MemoryRepresentation::Int32(),
                         RegisterRepresentation::Word32(), 4);
        };

        V<Word32> load0 = MakeLoad();

        V<HeapObject> fresh = __ FinishInitialization(
            __ Allocate(16, AllocationType::kSharedOld,
                        AllocationAlignment::kTaggedAligned));

        V<Word32> intervening = __ Load(
            fresh, {},
            LoadOp::Kind::TaggedBase().Atomic(AtomicMemoryOrder::kSeqCst),
            MemoryRepresentation::Int32(), RegisterRepresentation::Word32(), 8);

        V<Word32> C(load1) = MakeLoad();

        __ Return(__ Word32Constant(0),
                  base::VectorOf({load0, intervening, load1}));
      },
      /* is_wasm */ true);

  test.Run<LateLoadEliminationReducer>();

  const LoadOp* load1 = test.GetCapturedAs<LoadOp>("load1");

  // No load elimination if there exists an intervening atomic load.
  ASSERT_NE(load1, nullptr);
}

TEST_F(WasmLateLoadEliminationReducerTest, WasmInterveningMemoryBarrier) {
  for (AtomicMemoryOrder order :
       {AtomicMemoryOrder::kSeqCst, AtomicMemoryOrder::kAcqRel}) {
    auto test = CreateFromGraph(
        1,
        [order](auto& Asm) {
          auto MakeLoad = [&Asm]() -> V<Word32> {
            return __ Load(Asm.GetParameter(0), {}, LoadOp::Kind::TaggedBase(),
                           MemoryRepresentation::Int32(),
                           RegisterRepresentation::Word32(), 4);
          };

          V<Word32> load0 = MakeLoad();

          __ MemoryBarrier(order);

          V<Word32> C(load1) = MakeLoad();

          __ Return(__ Word32Constant(0), base::VectorOf({load0, load1}));
        },
        /* is_wasm */ true);

    test.Run<LateLoadEliminationReducer>();

    const LoadOp* load1 = test.GetCapturedAs<LoadOp>("load1");
    // No load elimination across a memory barrier.
    ASSERT_NE(load1, nullptr);
  }
}

TEST_F(WasmLateLoadEliminationReducerTest, WasmInterveningAtomicWord32Pair) {
  auto test_load = CreateFromGraph(
      base::VectorOf({RegisterRepresentation::Tagged(),
                      RegisterRepresentation::WordPtr()}),
      [](auto& Asm) {
        auto MakeLoad = [&Asm]() -> V<Word32> {
          return __ Load(Asm.GetParameter(0), {}, LoadOp::Kind::TaggedBase(),
                         MemoryRepresentation::Int32(),
                         RegisterRepresentation::Word32(), 4);
        };

        V<Word32> load0 = MakeLoad();
        __ AtomicWord32PairLoad(Asm.template GetParameter<WordPtr>(1), {}, 0);
        V<Word32> C(load1) = MakeLoad();

        __ Return(__ Word32Constant(0), base::VectorOf({load0, load1}));
      },
      /* is_wasm */ true);
  test_load.Run<LateLoadEliminationReducer>();
  ASSERT_NE(test_load.GetCapturedAs<LoadOp>("load1"), nullptr);

  auto test_store = CreateFromGraph(
      base::VectorOf({RegisterRepresentation::Tagged(),
                      RegisterRepresentation::WordPtr()}),
      [](auto& Asm) {
        auto MakeLoad = [&Asm]() -> V<Word32> {
          return __ Load(Asm.GetParameter(0), {}, LoadOp::Kind::TaggedBase(),
                         MemoryRepresentation::Int32(),
                         RegisterRepresentation::Word32(), 4);
        };

        V<Word32> load0 = MakeLoad();
        __ AtomicWord32PairStore(Asm.template GetParameter<WordPtr>(1), {},
                                 __ Word32Constant(1), __ Word32Constant(2), 0);
        V<Word32> C(load1) = MakeLoad();

        __ Return(__ Word32Constant(0), base::VectorOf({load0, load1}));
      },
      /* is_wasm */ true);
  test_store.Run<LateLoadEliminationReducer>();
  ASSERT_NE(test_store.GetCapturedAs<LoadOp>("load1"), nullptr);
}

class WasmLoadEliminationReducerTest : public ReducerTest {
 public:
  WasmLoadEliminationReducerTest()
      : ReducerTest(),
        flag_load_elimination_(&v8_flags.turboshaft_wasm_load_elimination,
                               true),
        flag_wasm_shared_(&v8_flags.wasm_shared, true) {}
  wasm::StructType* SetupModule(SharedFlag shared) {
    module_.reset(new wasm::WasmModule());
    wasm::StructType::Builder<Zone> type_builder(
        zone(), /* field_count */ 2, /* is_descriptor */ false, shared);
    type_builder.AddField(wasm::kWasmI32, true);
    type_builder.AddField(wasm::kWasmI32, true);
    wasm::StructType* type = type_builder.Build();

    module_->AddStructTypeForTesting(type, wasm::kNoSuperType, true, shared);
    return type;
  }

  static V<Any> MakeStructGet(
      TestInstance& Asm, wasm::StructType* struct_type,
      std::optional<AtomicMemoryOrder> memory_order = {}) {
    return __ StructGet(V<WasmStructNullable>::Cast(Asm.GetParameter(0)),
                        struct_type, {0}, 0, true, CheckForNull::kWithNullCheck,
                        memory_order);
  }

 private:
  const FlagScope<bool> flag_load_elimination_;
  const FlagScope<bool> flag_wasm_shared_;

 protected:
  std::unique_ptr<wasm::WasmModule> module_;
};

TEST_F(WasmLoadEliminationReducerTest, WasmTriviallyEliminatedStructGet) {
  wasm::StructType* struct_type = SetupModule(SharedFlag{true});
  auto test = CreateFromGraph(
      1,
      [struct_type](auto& Asm) {
        V<Any> get0 = MakeStructGet(Asm, struct_type);
        V<Any> C(get1) = MakeStructGet(Asm, struct_type);

        __ Return(__ Word32Constant(0), base::VectorOf({get0, get1}));
      },
      module_.get(),
      wasm::FunctionSig::Build(
          zone(), {wasm::kWasmI32, wasm::kWasmI32},
          {wasm::ValueType::Ref({0}, SharedFlag{true},
                                wasm::RefTypeKind::kStruct)}));

  test.Run<WasmLoadEliminationReducer>();

  // Baseline: Load elimination happens.
  ASSERT_TRUE(test.GetCapture("get1").IsEmpty());
}

TEST_F(WasmLoadEliminationReducerTest, WasmAtomicStructGetNoElimination) {
  wasm::StructType* struct_type = SetupModule(SharedFlag{true});
  auto test = CreateFromGraph(
      1,
      [struct_type](auto& Asm) {
        V<Any> get0 =
            MakeStructGet(Asm, struct_type, {AtomicMemoryOrder::kSeqCst});
        V<Any> C(get1) =
            MakeStructGet(Asm, struct_type, {AtomicMemoryOrder::kSeqCst});

        __ Return(__ Word32Constant(0), base::VectorOf({get0, get1}));
      },
      module_.get(),
      wasm::FunctionSig::Build(
          zone(), {wasm::kWasmI32, wasm::kWasmI32},
          {wasm::ValueType::Ref({0}, SharedFlag{true},
                                wasm::RefTypeKind::kStruct)}));

  test.Run<WasmLoadEliminationReducer>();

  const StructGetOp* get1 = test.GetCapturedAs<StructGetOp>("get1");
  // No load elimination for atomics.
  // TODO(manoskouk): Improve this.
  ASSERT_NE(get1, nullptr);
}

TEST_F(WasmLoadEliminationReducerTest,
       WasmStructGetInterveningAtomicStructSet) {
  wasm::StructType* struct_type = SetupModule(SharedFlag{true});
  auto test = CreateFromGraph(
      1,
      [struct_type](auto& Asm) {
        V<Any> get0 = MakeStructGet(Asm, struct_type);

        V<WasmStruct> fresh = __ WasmAllocateStruct(
            V<Map>::Cast(Asm.GetParameter(0)), struct_type, {0});

        __ StructSet(fresh, __ Word32Constant(1), struct_type, {0}, 0,
                     CheckForNull::kWithNullCheck, {AtomicMemoryOrder::kSeqCst},
                     WriteBarrierKind::kNoWriteBarrier,
                     StructSetOp::Kind::kAssign);

        V<Any> C(get1) = MakeStructGet(Asm, struct_type);

        __ Return(__ Word32Constant(0), base::VectorOf({get0, get1}));
      },
      module_.get(),
      wasm::FunctionSig::Build(
          zone(), {wasm::kWasmI32, wasm::kWasmI32},
          {wasm::ValueType::Ref({0}, SharedFlag{true},
                                wasm::RefTypeKind::kStruct)}));

  test.Run<WasmLoadEliminationReducer>();

  const StructGetOp* get1 = test.GetCapturedAs<StructGetOp>("get1");
  // No load elimination for an intervening atomic StructSet.
  ASSERT_NE(get1, nullptr);
}

TEST_F(WasmLoadEliminationReducerTest,
       WasmStructGetOnNonSharedStructInterveningAtomicStructSet) {
  wasm::StructType* struct_type = SetupModule(SharedFlag{false});
  auto test = CreateFromGraph(
      1,
      [struct_type](auto& Asm) {
        V<Any> get0 = MakeStructGet(Asm, struct_type);

        V<WasmStruct> fresh = __ WasmAllocateStruct(
            V<Map>::Cast(Asm.GetParameter(0)), struct_type, {0});

        __ StructSet(fresh, __ Word32Constant(1), struct_type, {0}, 1,
                     CheckForNull::kWithNullCheck, {AtomicMemoryOrder::kSeqCst},
                     WriteBarrierKind::kNoWriteBarrier,
                     StructSetOp::Kind::kAssign);

        V<Any> C(get1) = MakeStructGet(Asm, struct_type);

        __ Return(__ Word32Constant(0), base::VectorOf({get0, get1}));
      },
      module_.get(),
      wasm::FunctionSig::Build(
          zone(), {wasm::kWasmI32, wasm::kWasmI32},
          {wasm::ValueType::Ref({0}, SharedFlag{false},
                                wasm::RefTypeKind::kStruct)}));

  test.Run<WasmLoadEliminationReducer>();

  // Load elimination for non-shared struct happens.
  ASSERT_TRUE(test.GetCapture("get1").IsEmpty());
}

TEST_F(WasmLoadEliminationReducerTest,
       WasmStructGetInterveningStructSetOnFreshStruct) {
  wasm::StructType* struct_type = SetupModule(SharedFlag{false});
  auto test = CreateFromGraph(
      1,
      [struct_type](auto& Asm) {
        V<Any> get0 = MakeStructGet(Asm, struct_type);

        V<WasmStruct> fresh = __ WasmAllocateStruct(
            V<Map>::Cast(Asm.GetParameter(0)), struct_type, {0});

        __ StructSet(fresh, __ Word32Constant(1), struct_type, {0}, 0,
                     CheckForNull::kWithoutNullCheck, {},
                     WriteBarrierKind::kNoWriteBarrier,
                     StructSetOp::Kind::kInitialize);

        V<Any> C(get1) = MakeStructGet(Asm, struct_type);

        __ Return(__ Word32Constant(0), base::VectorOf({get0, get1}));
      },
      module_.get(),
      wasm::FunctionSig::Build(
          zone(), {wasm::kWasmI32, wasm::kWasmI32},
          {wasm::ValueType::Ref({0}, SharedFlag{false},
                                wasm::RefTypeKind::kStruct)}));

  test.Run<WasmLoadEliminationReducer>();

  // Load elimination happens because `fresh` is non-aliasing.
  ASSERT_TRUE(test.GetCapture("get1").IsEmpty());
}

TEST_F(WasmLoadEliminationReducerTest,
       WasmStructGetOnFreshStructAfterSupertypeStructSet) {
  module_.reset(new wasm::WasmModule());
  wasm::StructType::Builder<Zone> super_builder(zone(), /* field_count */ 1,
                                                /* is_descriptor */ false,
                                                SharedFlag{false});
  super_builder.AddField(wasm::kWasmI32, true);
  wasm::StructType* super_type = super_builder.Build();
  module_->AddStructTypeForTesting(super_type, wasm::kNoSuperType, false,
                                   SharedFlag{false});

  wasm::StructType::Builder<Zone> sub_builder(zone(), /* field_count */ 1,
                                              /* is_descriptor */ false,
                                              SharedFlag{false});
  sub_builder.AddField(wasm::kWasmI32, true);
  wasm::StructType* sub_type = sub_builder.Build();
  module_->AddStructTypeForTesting(sub_type, wasm::ModuleTypeIndex{0}, true,
                                   SharedFlag{false});

  auto test = CreateFromGraph(
      1,
      [super_type, sub_type](auto& Asm) {
        V<WasmStruct> fresh = __ WasmAllocateStruct(
            V<Map>::Cast(Asm.GetParameter(0)), sub_type, {1});

        __ StructSet(fresh, __ Word32Constant(10), sub_type, {1}, 0,
                     CheckForNull::kWithoutNullCheck, {},
                     WriteBarrierKind::kNoWriteBarrier,
                     StructSetOp::Kind::kInitialize);

        __ StructSet(fresh, __ Word32Constant(20), super_type, {0}, 0,
                     CheckForNull::kWithoutNullCheck, {},
                     WriteBarrierKind::kNoWriteBarrier,
                     StructSetOp::Kind::kAssign);

        V<Any> C(get) = __ StructGet(fresh, sub_type, {1}, 0, true,
                                     CheckForNull::kWithoutNullCheck, {});

        __ Return(__ Word32Constant(0), base::VectorOf({get}));
      },
      module_.get(),
      wasm::FunctionSig::Build(
          zone(), {wasm::kWasmI32},
          {wasm::ValueType::Ref({1}, SharedFlag{false},
                                wasm::RefTypeKind::kStruct)}));

  test.Run<WasmLoadEliminationReducer>();

  // The supertype StructSet must invalidate the subtype entry on `fresh`.
  const StructGetOp* get = test.GetCapturedAs<StructGetOp>("get");
  ASSERT_NE(get, nullptr);
}

TEST_F(WasmLoadEliminationReducerTest,
       WasmStructGetOnFreshNonSharedStructAfterAtomicStructSet) {
  wasm::StructType* struct_type = SetupModule(SharedFlag{false});
  auto test = CreateFromGraph(
      1,
      [struct_type](auto& Asm) {
        V<WasmStruct> fresh = __ WasmAllocateStruct(
            V<Map>::Cast(Asm.GetParameter(0)), struct_type, {0});

        __ StructSet(fresh, __ Word32Constant(10), struct_type, {0}, 0,
                     CheckForNull::kWithoutNullCheck, {},
                     WriteBarrierKind::kNoWriteBarrier,
                     StructSetOp::Kind::kInitialize);

        __ StructSet(
            fresh, __ Word32Constant(20), struct_type, {0}, 0,
            CheckForNull::kWithoutNullCheck, {AtomicMemoryOrder::kSeqCst},
            WriteBarrierKind::kNoWriteBarrier, StructSetOp::Kind::kAssign);

        V<Any> C(get) = __ StructGet(fresh, struct_type, {0}, 0, true,
                                     CheckForNull::kWithoutNullCheck, {});

        __ Return(__ Word32Constant(0), base::VectorOf({get}));
      },
      module_.get(),
      wasm::FunctionSig::Build(
          zone(), {wasm::kWasmI32},
          {wasm::ValueType::Ref({0}, SharedFlag{false},
                                wasm::RefTypeKind::kStruct)}));

  test.Run<WasmLoadEliminationReducer>();

  // The atomic StructSet must invalidate the entry on `fresh`.
  const StructGetOp* get = test.GetCapturedAs<StructGetOp>("get");
  ASSERT_NE(get, nullptr);
}

TEST_F(WasmLoadEliminationReducerTest,
       WasmStructGetOnFreshStructEscapedViaAssertNotNull) {
  wasm::StructType* struct_type = SetupModule(SharedFlag{false});
  wasm::WasmGlobal global{
      .type = wasm::ValueType::RefNull({0}, SharedFlag{false},
                                       wasm::RefTypeKind::kStruct),
      .mutability = true,
      .index_in_buffer = 0};
  auto test = CreateFromGraph(
      1,
      [struct_type, &global](auto& Asm) {
        V<WasmStruct> fresh = __ WasmAllocateStruct(
            V<Map>::Cast(Asm.GetParameter(0)), struct_type, {0});

        __ StructSet(fresh, __ Word32Constant(10), struct_type, {0}, 0,
                     CheckForNull::kWithoutNullCheck, {},
                     WriteBarrierKind::kNoWriteBarrier,
                     StructSetOp::Kind::kInitialize);

        wasm::ValueType nullable_struct_ref = wasm::ValueType::RefNull(
            {0}, SharedFlag{false}, wasm::RefTypeKind::kStruct);
        wasm::ValueType struct_ref = wasm::ValueType::Ref(
            {0}, SharedFlag{false}, wasm::RefTypeKind::kStruct);
        V<Object> non_null = __ AssertNotNull(fresh, nullable_struct_ref,
                                              TrapId::kTrapNullDereference);
        V<Object> annotated = __ AnnotateWasmType(non_null, struct_ref);
        V<Object> casted = __ WasmTypeCast(
            annotated, V<Map>::Cast(Asm.GetParameter(0)),
            WasmTypeCheckConfig{nullable_struct_ref, struct_ref});
        __ GlobalSet(V<WasmTrustedInstanceData>::Cast(Asm.GetParameter(0)),
                     casted, &global);

        // A StructSet on an arbitrary struct can now alias `fresh`.
        __ StructSet(V<WasmStructNullable>::Cast(Asm.GetParameter(0)),
                     __ Word32Constant(20), struct_type, {0}, 0,
                     CheckForNull::kWithNullCheck, {},
                     WriteBarrierKind::kNoWriteBarrier,
                     StructSetOp::Kind::kAssign);

        V<Any> C(get) = __ StructGet(fresh, struct_type, {0}, 0, true,
                                     CheckForNull::kWithoutNullCheck, {});

        __ Return(__ Word32Constant(0), base::VectorOf({get}));
      },
      module_.get(),
      wasm::FunctionSig::Build(
          zone(), {wasm::kWasmI32},
          {wasm::ValueType::Ref({0}, SharedFlag{false},
                                wasm::RefTypeKind::kStruct)}));

  test.Run<WasmLoadEliminationReducer>();

  // `fresh` escaped via `AssertNotNull`, `WasmTypeAnnotation`, and
  // `WasmTypeCast` into `GlobalSet`, so the `StructSet` must invalidate its
  // field.
  const StructGetOp* get = test.GetCapturedAs<StructGetOp>("get");
  ASSERT_NE(get, nullptr);
}

TEST_F(WasmLoadEliminationReducerTest,
       WasmStructGetOnFreshStructWithTypeCastAndAnnotation) {
  wasm::StructType* struct_type = SetupModule(SharedFlag{false});
  auto test = CreateFromGraph(
      1,
      [struct_type](auto& Asm) {
        wasm::ValueType nullable_struct_ref = wasm::ValueType::RefNull(
            {0}, SharedFlag{false}, wasm::RefTypeKind::kStruct);
        wasm::ValueType struct_ref = wasm::ValueType::Ref(
            {0}, SharedFlag{false}, wasm::RefTypeKind::kStruct);
        V<WasmStruct> fresh = __ WasmAllocateStruct(
            V<Map>::Cast(Asm.GetParameter(0)), struct_type, {0});

        __ StructSet(fresh, __ Word32Constant(10), struct_type, {0}, 0,
                     CheckForNull::kWithoutNullCheck, {},
                     WriteBarrierKind::kNoWriteBarrier,
                     StructSetOp::Kind::kInitialize);

        V<Word32> is_null = __ IsNull(fresh, nullable_struct_ref);
        V<Word32> type_check = __ WasmTypeCheck(
            fresh, V<Map>::Cast(Asm.GetParameter(0)),
            WasmTypeCheckConfig{nullable_struct_ref, struct_ref});
        V<Object> annotated = __ AnnotateWasmType(fresh, struct_ref);
        V<Object> casted = __ WasmTypeCast(
            annotated, V<Map>::Cast(Asm.GetParameter(0)),
            WasmTypeCheckConfig{nullable_struct_ref, struct_ref});

        // A StructSet on another struct must not invalidate `fresh` because
        // `IsNull`, `WasmTypeCheck`, `WasmTypeAnnotation`, and `WasmTypeCast`
        // do not create escaping aliases.
        __ StructSet(V<WasmStructNullable>::Cast(Asm.GetParameter(0)),
                     __ Word32Constant(20), struct_type, {0}, 0,
                     CheckForNull::kWithNullCheck, {},
                     WriteBarrierKind::kNoWriteBarrier,
                     StructSetOp::Kind::kAssign);

        V<Any> C(get) =
            __ StructGet(V<WasmStructNullable>::Cast(casted), struct_type, {0},
                         0, true, CheckForNull::kWithoutNullCheck, {});

        __ Return(__ Word32Constant(0),
                  base::VectorOf<V<Any>>({is_null, type_check, get}));
      },
      module_.get(),
      wasm::FunctionSig::Build(
          zone(), {wasm::kWasmI32, wasm::kWasmI32, wasm::kWasmI32},
          {wasm::ValueType::Ref({0}, SharedFlag{false},
                                wasm::RefTypeKind::kStruct)}));

  test.Run<WasmLoadEliminationReducer>();

  // Load elimination happens because `fresh` remains non-aliasing.
  ASSERT_TRUE(test.GetCapture("get").IsEmpty());
}

TEST_F(WasmLoadEliminationReducerTest, WasmStructGetInterveningAtomicStore) {
  wasm::StructType* struct_type = SetupModule(SharedFlag{true});
  auto test = CreateFromGraph(
      base::VectorOf({RegisterRepresentation::Tagged(),
                      RegisterRepresentation::WordPtr()}),
      [struct_type](auto& Asm) {
        V<Any> get0 = MakeStructGet(Asm, struct_type);

        __ Store(Asm.template GetParameter<WordPtr>(1), {},
                 __ Word32Constant(1),
                 StoreOp::Kind::RawAligned().Atomic(AtomicMemoryOrder::kSeqCst),
                 MemoryRepresentation::Int32(),
                 WriteBarrierKind::kNoWriteBarrier, 8);

        V<Any> C(get1) = MakeStructGet(Asm, struct_type);

        __ Return(__ Word32Constant(0), base::VectorOf({get0, get1}));
      },
      module_.get(),
      wasm::FunctionSig::Build(
          zone(), {wasm::kWasmI32, wasm::kWasmI32},
          {wasm::ValueType::Ref({0}, SharedFlag{true},
                                wasm::RefTypeKind::kStruct)}));

  test.Run<WasmLoadEliminationReducer>();

  const StructGetOp* get1 = test.GetCapturedAs<StructGetOp>("get1");
  // No load elimination for an intervening atomic Store.
  ASSERT_NE(get1, nullptr);
}

TEST_F(WasmLoadEliminationReducerTest, WasmCatchBlockBeginPreservesStructGet) {
  wasm::StructType* struct_type = SetupModule(SharedFlag{false});
  auto test = CreateFromGraph(
      1,
      [struct_type](auto& Asm) {
        V<Any> get0 = MakeStructGet(Asm, struct_type);

        Block* catch_block = __ NewBlock();
        V<String> str;
        {
          CatchScopeImpl<assembler_t> scope(Asm.Asm(), catch_block);
          str = __ template WasmCallBuiltinThroughJumptable<
              deprecated::BuiltinCallDescriptor::WasmIntToString>(
              {__ Word32Constant(0), __ Word32Constant(10)});
        }
        __ Return(__ Word32Constant(0), base::VectorOf<OpIndex>({get0, str}));

        if (__ Bind(catch_block)) {
          __ CatchBlockBegin();
          V<Any> C(get1) = MakeStructGet(Asm, struct_type);
          __ Return(__ Word32Constant(0), base::VectorOf({get0, get1}));
        }
      },
      module_.get(),
      wasm::FunctionSig::Build(
          zone(), {wasm::kWasmI32, wasm::kWasmI32},
          {wasm::ValueType::Ref({0}, SharedFlag{false},
                                wasm::RefTypeKind::kStruct)}));

  test.Run<WasmLoadEliminationReducer>();

  if (!LATE_LOAD_ELIM_VERIFY) {
    ASSERT_TRUE(test.GetCapture("get1").IsEmpty());
  }
}

TEST_F(WasmLoadEliminationReducerTest, WasmStructGetInterveningMemoryBarrier) {
  for (AtomicMemoryOrder order :
       {AtomicMemoryOrder::kSeqCst, AtomicMemoryOrder::kAcqRel}) {
    wasm::StructType* struct_type = SetupModule(SharedFlag{true});
    auto test = CreateFromGraph(
        1,
        [struct_type, order](auto& Asm) {
          V<Any> get0 = MakeStructGet(Asm, struct_type);
          __ MemoryBarrier(order);
          V<Any> C(get1) = MakeStructGet(Asm, struct_type);

          __ Return(__ Word32Constant(0), base::VectorOf({get0, get1}));
        },
        module_.get(),
        wasm::FunctionSig::Build(
            zone(), {wasm::kWasmI32, wasm::kWasmI32},
            {wasm::ValueType::Ref({0}, SharedFlag{true},
                                  wasm::RefTypeKind::kStruct)}));

    test.Run<WasmLoadEliminationReducer>();

    const StructGetOp* get1 = test.GetCapturedAs<StructGetOp>("get1");
    // No load elimination for a shared struct across a memory barrier.
    ASSERT_NE(get1, nullptr);
  }
}

TEST_F(WasmLoadEliminationReducerTest,
       WasmStructGetOnNonSharedStructInterveningMemoryBarrier) {
  wasm::StructType* struct_type = SetupModule(SharedFlag{false});
  auto test = CreateFromGraph(
      1,
      [struct_type](auto& Asm) {
        V<Any> get0 = MakeStructGet(Asm, struct_type);
        __ MemoryBarrier(AtomicMemoryOrder::kSeqCst);
        V<Any> C(get1) = MakeStructGet(Asm, struct_type);

        __ Return(__ Word32Constant(0), base::VectorOf({get0, get1}));
      },
      module_.get(),
      wasm::FunctionSig::Build(
          zone(), {wasm::kWasmI32, wasm::kWasmI32},
          {wasm::ValueType::Ref({0}, SharedFlag{false},
                                wasm::RefTypeKind::kStruct)}));

  test.Run<WasmLoadEliminationReducer>();

  // Non-shared struct loads can still be eliminated across a memory barrier.
  if (!LATE_LOAD_ELIM_VERIFY) {
    ASSERT_TRUE(test.GetCapture("get1").IsEmpty());
  }
}

#endif  // V8_ENABLE_WEBASSEMBLY

#undef C

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft
