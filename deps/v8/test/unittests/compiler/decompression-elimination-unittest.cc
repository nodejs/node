// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/decompression-elimination.h"
#include "src/compiler/simplified-operator.h"
#include "test/unittests/compiler/graph-reducer-unittest.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"

using testing::StrictMock;

namespace v8 {
namespace internal {
namespace compiler {

class DecompressionEliminationTest : public GraphTest {
 public:
  DecompressionEliminationTest()
      : GraphTest(),
        machine_(zone(), MachineType::PointerRepresentation(),
                 MachineOperatorBuilder::kNoFlags),
        simplified_(zone()) {}
  ~DecompressionEliminationTest() override = default;

 protected:
  Reduction Reduce(Node* node) {
    StrictMock<MockAdvancedReducerEditor> editor;
    DecompressionElimination decompression_elimination(&editor, graph(),
                                                       machine(), common());
    return decompression_elimination.Reduce(node);
  }
  MachineOperatorBuilder* machine() { return &machine_; }
  SimplifiedOperatorBuilder* simplified() { return &simplified_; }

 private:
  MachineOperatorBuilder machine_;
  SimplifiedOperatorBuilder simplified_;
};

// -----------------------------------------------------------------------------
// Direct Decompression & Compression

TEST_F(DecompressionEliminationTest, BasicDecompressionCompression) {
  // Skip test if pointer compression is not enabled
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  ElementAccess const access = {kTaggedBase, kTaggedSize, Type::Any(),
                                MachineType::AnyTagged(), kNoWriteBarrier};

  // Create the graph
  Node* load = graph()->NewNode(simplified()->LoadElement(access), object,
                                index, effect, control);
  Node* changeToTagged =
      graph()->NewNode(machine()->ChangeCompressedToTagged(), load);
  Node* changeToCompressed =
      graph()->NewNode(machine()->ChangeTaggedToCompressed(), changeToTagged);
  effect = graph()->NewNode(simplified()->StoreElement(access), object, index,
                            changeToCompressed, effect, control);

  // Reduce
  Reduction r = Reduce(changeToCompressed);
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(load, r.replacement());
}

TEST_F(DecompressionEliminationTest, BasicDecompressionCompressionSigned) {
  // Skip test if pointer compression is not enabled
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  ElementAccess const access = {kTaggedBase, kTaggedSize, Type::Any(),
                                MachineType::TaggedSigned(), kNoWriteBarrier};

  // Create the graph
  Node* load = graph()->NewNode(simplified()->LoadElement(access), object,
                                index, effect, control);
  Node* changeToTagged =
      graph()->NewNode(machine()->ChangeCompressedSignedToTaggedSigned(), load);
  Node* changeToCompressed = graph()->NewNode(
      machine()->ChangeTaggedSignedToCompressedSigned(), changeToTagged);
  effect = graph()->NewNode(simplified()->StoreElement(access), object, index,
                            changeToCompressed, effect, control);

  // Reduce
  Reduction r = Reduce(changeToCompressed);
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(load, r.replacement());
}

TEST_F(DecompressionEliminationTest, BasicDecompressionCompressionPointer) {
  // Skip test if pointer compression is not enabled
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  ElementAccess const access = {kTaggedBase, kTaggedSize, Type::Any(),
                                MachineType::TaggedPointer(), kNoWriteBarrier};

  // Create the graph
  Node* load = graph()->NewNode(simplified()->LoadElement(access), object,
                                index, effect, control);
  Node* changeToTagged = graph()->NewNode(
      machine()->ChangeCompressedPointerToTaggedPointer(), load);
  Node* changeToCompressed = graph()->NewNode(
      machine()->ChangeTaggedPointerToCompressedPointer(), changeToTagged);
  effect = graph()->NewNode(simplified()->StoreElement(access), object, index,
                            changeToCompressed, effect, control);

  // Reduce
  Reduction r = Reduce(changeToCompressed);
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(load, r.replacement());
}

// -----------------------------------------------------------------------------
// Direct Decompression & Compression - border cases

// For example, if we are lowering a CheckedCompressedToTaggedPointer in the
// effect linearization phase we will change that to
// ChangeCompressedPointerToTaggedPointer. Then, we might end up with a chain of
// Parent <- ChangeCompressedPointerToTaggedPointer <- ChangeTaggedToCompressed
// <- Child.
// Similarly, we have cases with Signed instead of pointer.
// The following border case tests will test that the functionality is robust
// enough to handle that.

TEST_F(DecompressionEliminationTest,
       BasicDecompressionCompressionBorderCaseSigned) {
  // Skip test if pointer compression is not enabled
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  ElementAccess const loadAccess = {kTaggedBase, kTaggedSize, Type::Any(),
                                    MachineType::AnyTagged(), kNoWriteBarrier};
  ElementAccess const storeAccess = {kTaggedBase, kTaggedSize, Type::Any(),
                                     MachineType::TaggedSigned(),
                                     kNoWriteBarrier};

  // Create the graph
  Node* load = graph()->NewNode(simplified()->LoadElement(loadAccess), object,
                                index, effect, control);
  Node* changeToTagged =
      graph()->NewNode(machine()->ChangeCompressedSignedToTaggedSigned(), load);
  Node* changeToCompressed =
      graph()->NewNode(machine()->ChangeTaggedToCompressed(), changeToTagged);
  effect = graph()->NewNode(simplified()->StoreElement(storeAccess), object,
                            index, changeToCompressed, effect, control);

  // Reduce
  Reduction r = Reduce(changeToCompressed);
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(load, r.replacement());
}

TEST_F(DecompressionEliminationTest,
       BasicDecompressionCompressionBorderCasePointer) {
  // Skip test if pointer compression is not enabled
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  ElementAccess const loadAccess = {kTaggedBase, kTaggedSize, Type::Any(),
                                    MachineType::AnyTagged(), kNoWriteBarrier};
  ElementAccess const storeAccess = {kTaggedBase, kTaggedSize, Type::Any(),
                                     MachineType::TaggedPointer(),
                                     kNoWriteBarrier};

  // Create the graph
  Node* load = graph()->NewNode(simplified()->LoadElement(loadAccess), object,
                                index, effect, control);
  Node* changeToTagged = graph()->NewNode(
      machine()->ChangeCompressedPointerToTaggedPointer(), load);
  Node* changeToCompressed =
      graph()->NewNode(machine()->ChangeTaggedToCompressed(), changeToTagged);
  effect = graph()->NewNode(simplified()->StoreElement(storeAccess), object,
                            index, changeToCompressed, effect, control);

  // Reduce
  Reduction r = Reduce(changeToCompressed);
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(load, r.replacement());
}

// We also have cases of ChangeCompressedToTagged <-
// ChangeTaggedPointerToCompressedPointer, where the
// ChangeTaggedPointerToCompressedPointer was introduced while lowering a
// NewConsString on effect control linearizer

TEST_F(DecompressionEliminationTest,
       BasicDecompressionCompressionBorderCasePointerDecompression) {
  // Skip test if pointer compression is not enabled
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  ElementAccess const loadAccess = {kTaggedBase, kTaggedSize, Type::Any(),
                                    MachineType::TaggedPointer(),
                                    kNoWriteBarrier};
  ElementAccess const storeAccess = {kTaggedBase, kTaggedSize, Type::Any(),
                                     MachineType::AnyTagged(), kNoWriteBarrier};

  // Create the graph
  Node* load = graph()->NewNode(simplified()->LoadElement(loadAccess), object,
                                index, effect, control);
  Node* changeToTagged = graph()->NewNode(
      machine()->ChangeCompressedPointerToTaggedPointer(), load);
  Node* changeToCompressed =
      graph()->NewNode(machine()->ChangeTaggedToCompressed(), changeToTagged);
  effect = graph()->NewNode(simplified()->StoreElement(storeAccess), object,
                            index, changeToCompressed, effect, control);

  // Reduce
  Reduction r = Reduce(changeToCompressed);
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(load, r.replacement());
}

// -----------------------------------------------------------------------------
// Compress after constant

TEST_F(DecompressionEliminationTest,
       DecompressionConstantStoreElementInt64Constant) {
  // Skip test if pointer compression is not enabled.
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);

  const ElementAccess element_accesses[] = {
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::AnyCompressed(),
       kNoWriteBarrier},
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::CompressedSigned(),
       kNoWriteBarrier},
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::CompressedPointer(),
       kNoWriteBarrier}};

  const Operator* compression_ops[] = {
      machine()->ChangeTaggedToCompressed(),
      machine()->ChangeTaggedSignedToCompressedSigned(),
      machine()->ChangeTaggedPointerToCompressedPointer()};

  ASSERT_EQ(arraysize(compression_ops), arraysize(element_accesses));

  const int64_t constants[] = {static_cast<int64_t>(0x0000000000000000),
                               static_cast<int64_t>(0x0000000000000001),
                               static_cast<int64_t>(0x0000FFFFFFFF0000),
                               static_cast<int64_t>(0x7FFFFFFFFFFFFFFF),
                               static_cast<int64_t>(0x8000000000000000),
                               static_cast<int64_t>(0x8000000000000001),
                               static_cast<int64_t>(0x8000FFFFFFFF0000),
                               static_cast<int64_t>(0x8FFFFFFFFFFFFFFF),
                               static_cast<int64_t>(0xFFFFFFFFFFFFFFFF)};

  // For every compression.
  for (size_t i = 0; i < arraysize(compression_ops); ++i) {
    // For every Int64Constant.
    for (size_t j = 0; j < arraysize(constants); ++j) {
      // Create the graph.
      Node* constant = graph()->NewNode(common()->Int64Constant(constants[j]));
      Node* changeToCompressed = graph()->NewNode(compression_ops[i], constant);
      effect =
          graph()->NewNode(simplified()->StoreElement(element_accesses[i]),
                           object, index, changeToCompressed, effect, control);
      // Reduce.
      Reduction r = Reduce(changeToCompressed);
      ASSERT_TRUE(r.Changed());
      EXPECT_EQ(r.replacement()->opcode(), IrOpcode::kInt32Constant);
    }
  }
}

TEST_F(DecompressionEliminationTest,
       DecompressionConstantStoreElementHeapConstant) {
  // TODO(v8:8977): Disabling HeapConstant until CompressedHeapConstant
  // exists, since it breaks with verify CSA on.
  if (COMPRESS_POINTERS_BOOL) {
    return;
  }
  // Skip test if pointer compression is not enabled.
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);

  const ElementAccess element_accesses[] = {
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::AnyCompressed(),
       kNoWriteBarrier},
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::CompressedSigned(),
       kNoWriteBarrier},
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::CompressedPointer(),
       kNoWriteBarrier}};

  const Operator* compression_ops[] = {
      machine()->ChangeTaggedToCompressed(),
      machine()->ChangeTaggedSignedToCompressedSigned(),
      machine()->ChangeTaggedPointerToCompressedPointer()};

  ASSERT_EQ(arraysize(compression_ops), arraysize(element_accesses));

  const Handle<HeapNumber> heap_constants[] = {
      factory()->NewHeapNumber(0.0),
      factory()->NewHeapNumber(-0.0),
      factory()->NewHeapNumber(11.2),
      factory()->NewHeapNumber(-11.2),
      factory()->NewHeapNumber(3.1415 + 1.4142),
      factory()->NewHeapNumber(3.1415 - 1.4142),
      factory()->NewHeapNumber(0x0000000000000000),
      factory()->NewHeapNumber(0x0000000000000001),
      factory()->NewHeapNumber(0x0000FFFFFFFF0000),
      factory()->NewHeapNumber(0x7FFFFFFFFFFFFFFF),
      factory()->NewHeapNumber(0x8000000000000000),
      factory()->NewHeapNumber(0x8000000000000001),
      factory()->NewHeapNumber(0x8000FFFFFFFF0000),
      factory()->NewHeapNumber(0x8FFFFFFFFFFFFFFF),
      factory()->NewHeapNumber(0xFFFFFFFFFFFFFFFF)};

  // For every compression.
  for (size_t i = 0; i < arraysize(compression_ops); ++i) {
    // For every HeapNumber.
    for (size_t j = 0; j < arraysize(heap_constants); ++j) {
      // Create the graph.
      Node* constant =
          graph()->NewNode(common()->HeapConstant(heap_constants[j]));
      Node* changeToCompressed = graph()->NewNode(compression_ops[i], constant);
      effect =
          graph()->NewNode(simplified()->StoreElement(element_accesses[i]),
                           object, index, changeToCompressed, effect, control);
      // Reduce.
      Reduction r = Reduce(changeToCompressed);
      ASSERT_TRUE(r.Changed());
      // TODO(v8:8977): Change the IrOpcode here to kCompressedHeapConstant when
      // that is in place.
      EXPECT_EQ(r.replacement()->opcode(), IrOpcode::kHeapConstant);
    }
  }
}

// -----------------------------------------------------------------------------
// Phi

TEST_F(DecompressionEliminationTest, PhiOneDecompress) {
  // Skip test if pointer compression is not enabled
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  const int number_of_inputs = 1;

  const Operator* decompression_ops[] = {
      machine()->ChangeCompressedToTagged(),
      machine()->ChangeCompressedSignedToTaggedSigned(),
      machine()->ChangeCompressedPointerToTaggedPointer()};

  const ElementAccess element_accesses[] = {
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::AnyCompressed(),
       kNoWriteBarrier},
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::TaggedSigned(),
       kNoWriteBarrier},
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::TaggedPointer(),
       kNoWriteBarrier}};

  const IrOpcode::Value opcodes[] = {
      IrOpcode::kChangeCompressedToTagged,
      IrOpcode::kChangeCompressedSignedToTaggedSigned,
      IrOpcode::kChangeCompressedPointerToTaggedPointer};

  ASSERT_EQ(arraysize(decompression_ops), arraysize(element_accesses));
  ASSERT_EQ(arraysize(opcodes), arraysize(element_accesses));

  // For every access
  for (size_t i = 0; i < arraysize(element_accesses); ++i) {
    // Create the graph
    Node* load =
        graph()->NewNode(simplified()->LoadElement(element_accesses[i]), object,
                         index, effect, control);
    Node* change_to_tagged = graph()->NewNode(decompression_ops[i], load);
    Node* phi = graph()->NewNode(
        common()->Phi(MachineRepresentation::kTagged, number_of_inputs),
        change_to_tagged, control);

    // Reduce
    Reduction r = Reduce(phi);
    ASSERT_TRUE(r.Changed());
    EXPECT_EQ(opcodes[i], r.replacement()->opcode());
  }
}

TEST_F(DecompressionEliminationTest, PhiThreeDecompressSameRepresentation) {
  // Skip test if pointer compression is not enabled
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  const int number_of_inputs = 3;

  const Operator* decompression_ops[] = {
      machine()->ChangeCompressedToTagged(),
      machine()->ChangeCompressedSignedToTaggedSigned(),
      machine()->ChangeCompressedPointerToTaggedPointer()};

  const ElementAccess element_accesses[] = {
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::AnyCompressed(),
       kNoWriteBarrier},
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::CompressedSigned(),
       kNoWriteBarrier},
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::CompressedPointer(),
       kNoWriteBarrier}};

  const IrOpcode::Value opcodes[] = {
      IrOpcode::kChangeCompressedToTagged,
      IrOpcode::kChangeCompressedSignedToTaggedSigned,
      IrOpcode::kChangeCompressedPointerToTaggedPointer};

  ASSERT_EQ(arraysize(decompression_ops), arraysize(element_accesses));
  ASSERT_EQ(arraysize(opcodes), arraysize(element_accesses));

  // For every access
  for (size_t i = 0; i < arraysize(element_accesses); ++i) {
    // Create the graph
    Node* load1 =
        graph()->NewNode(simplified()->LoadElement(element_accesses[i]), object,
                         index, effect, control);
    Node* load2 =
        graph()->NewNode(simplified()->LoadElement(element_accesses[i]), object,
                         index, effect, control);
    Node* load3 =
        graph()->NewNode(simplified()->LoadElement(element_accesses[i]), object,
                         index, effect, control);
    Node* change_to_tagged1 = graph()->NewNode(decompression_ops[i], load1);
    Node* change_to_tagged2 = graph()->NewNode(decompression_ops[i], load2);
    Node* change_to_tagged3 = graph()->NewNode(decompression_ops[i], load3);

    Node* phi = graph()->NewNode(
        common()->Phi(MachineRepresentation::kTagged, number_of_inputs),
        change_to_tagged1, change_to_tagged2, change_to_tagged3, control);

    // Reduce
    Reduction r = Reduce(phi);
    ASSERT_TRUE(r.Changed());
    EXPECT_EQ(opcodes[i], r.replacement()->opcode());
  }
}

TEST_F(DecompressionEliminationTest, PhiThreeDecompressOneAnyRepresentation) {
  // Skip test if pointer compression is not enabled
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  const int number_of_inputs = 3;

  const Operator* decompression_ops[] = {
      machine()->ChangeCompressedSignedToTaggedSigned(),
      machine()->ChangeCompressedPointerToTaggedPointer()};

  const ElementAccess element_accesses[] = {
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::CompressedSigned(),
       kNoWriteBarrier},
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::CompressedPointer(),
       kNoWriteBarrier}};

  const ElementAccess any_access = {kTaggedBase, kTaggedSize, Type::Any(),
                                    MachineType::AnyCompressed(),
                                    kNoWriteBarrier};

  ASSERT_EQ(arraysize(decompression_ops), arraysize(element_accesses));

  // For every access
  for (size_t i = 0; i < arraysize(element_accesses); ++i) {
    // Create the graph
    Node* load1 =
        graph()->NewNode(simplified()->LoadElement(element_accesses[i]), object,
                         index, effect, control);
    Node* load2 =
        graph()->NewNode(simplified()->LoadElement(element_accesses[i]), object,
                         index, effect, control);
    // Note that load3 loads a CompressedAny instead of element_accesses[i]
    Node* load3 = graph()->NewNode(simplified()->LoadElement(any_access),
                                   object, index, effect, control);
    Node* change_to_tagged1 = graph()->NewNode(decompression_ops[i], load1);
    Node* change_to_tagged2 = graph()->NewNode(decompression_ops[i], load2);
    Node* change_to_tagged3 =
        graph()->NewNode(machine()->ChangeCompressedToTagged(), load3);

    Node* phi = graph()->NewNode(
        common()->Phi(MachineRepresentation::kTagged, number_of_inputs),
        change_to_tagged1, change_to_tagged2, change_to_tagged3, control);

    // Reduce
    Reduction r = Reduce(phi);
    ASSERT_TRUE(r.Changed());
    EXPECT_EQ(IrOpcode::kChangeCompressedToTagged, r.replacement()->opcode());
  }
}

TEST_F(DecompressionEliminationTest, PhiThreeInputsOneNotDecompressed) {
  // Skip test if pointer compression is not enabled
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  const int number_of_inputs = 3;

  const Operator* decompression_ops[] = {
      machine()->ChangeCompressedToTagged(),
      machine()->ChangeCompressedSignedToTaggedSigned(),
      machine()->ChangeCompressedPointerToTaggedPointer()};

  const ElementAccess element_accesses[] = {
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::AnyCompressed(),
       kNoWriteBarrier},
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::CompressedSigned(),
       kNoWriteBarrier},
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::CompressedPointer(),
       kNoWriteBarrier}};

  const IrOpcode::Value opcodes[] = {
      IrOpcode::kChangeCompressedToTagged,
      IrOpcode::kChangeCompressedSignedToTaggedSigned,
      IrOpcode::kChangeCompressedPointerToTaggedPointer};

  ASSERT_EQ(arraysize(decompression_ops), arraysize(element_accesses));
  ASSERT_EQ(arraysize(opcodes), arraysize(element_accesses));

  // For every access
  for (size_t i = 0; i < arraysize(element_accesses); ++i) {
    // Create the graph
    Node* load1 =
        graph()->NewNode(simplified()->LoadElement(element_accesses[i]), object,
                         index, effect, control);
    Node* load2 =
        graph()->NewNode(simplified()->LoadElement(element_accesses[i]), object,
                         index, effect, control);
    Node* load3 =
        graph()->NewNode(simplified()->LoadElement(element_accesses[i]), object,
                         index, effect, control);
    Node* change_to_tagged1 = graph()->NewNode(decompression_ops[i], load1);
    Node* change_to_tagged2 = graph()->NewNode(decompression_ops[i], load2);

    Node* phi = graph()->NewNode(
        common()->Phi(MachineRepresentation::kTagged, number_of_inputs),
        change_to_tagged1, change_to_tagged2, load3, control);

    // Reduce
    Reduction r = Reduce(phi);
    ASSERT_FALSE(r.Changed());
  }
}

// In the case of having one decompress Signed and one Pointer, we have to
// generate the conservative decompress any after the Phi.
TEST_F(DecompressionEliminationTest, PhiTwoDecompressesOneSignedOnePointer) {
  // Skip test if pointer compression is not enabled
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  const int number_of_inputs = 2;
  const ElementAccess signed_access = {kTaggedBase, kTaggedSize, Type::Any(),
                                       MachineType::CompressedSigned(),
                                       kNoWriteBarrier};
  const ElementAccess pointer_access = {kTaggedBase, kTaggedSize, Type::Any(),
                                        MachineType::CompressedPointer(),
                                        kNoWriteBarrier};

  // Create the graph
  Node* load1 = graph()->NewNode(simplified()->LoadElement(signed_access),
                                 object, index, effect, control);
  Node* load2 = graph()->NewNode(simplified()->LoadElement(pointer_access),
                                 object, index, effect, control);
  Node* change_to_tagged1 = graph()->NewNode(
      machine()->ChangeCompressedSignedToTaggedSigned(), load1);
  Node* change_to_tagged2 = graph()->NewNode(
      machine()->ChangeCompressedPointerToTaggedPointer(), load2);

  Node* phi = graph()->NewNode(
      common()->Phi(MachineRepresentation::kTagged, number_of_inputs),
      change_to_tagged1, change_to_tagged2, control);

  // Reduce
  Reduction r = Reduce(phi);
  ASSERT_TRUE(r.Changed());
  EXPECT_EQ(IrOpcode::kChangeCompressedToTagged, r.replacement()->opcode());
}

// -----------------------------------------------------------------------------
// TypedStateValues

TEST_F(DecompressionEliminationTest, TypedStateValuesOneDecompress) {
  // Skip test if pointer compression is not enabled
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  const int numberOfInputs = 1;
  const ZoneVector<MachineType>* types =
      new (graph()->zone()->New(sizeof(ZoneVector<MachineType>)))
          ZoneVector<MachineType>(numberOfInputs, graph()->zone());
  SparseInputMask dense = SparseInputMask::Dense();

  const ElementAccess ElementAccesses[] = {
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::AnyTagged(),
       kNoWriteBarrier},
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::TaggedSigned(),
       kNoWriteBarrier},
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::TaggedPointer(),
       kNoWriteBarrier}};

  // For every access
  for (size_t i = 0; i < arraysize(ElementAccesses); ++i) {
    // Create the graph
    Node* load = graph()->NewNode(simplified()->LoadElement(ElementAccesses[i]),
                                  object, index, effect, control);
    Node* changeToTagged = graph()->NewNode(
        machine()->ChangeCompressedPointerToTaggedPointer(), load);
    Node* typedStateValuesOneDecompress = graph()->NewNode(
        common()->TypedStateValues(types, dense), changeToTagged);

    // Reduce
    StrictMock<MockAdvancedReducerEditor> editor;
    DecompressionElimination decompression_elimination(&editor, graph(),
                                                       machine(), common());
    Reduction r =
        decompression_elimination.Reduce(typedStateValuesOneDecompress);
    ASSERT_TRUE(r.Changed());
  }
}

TEST_F(DecompressionEliminationTest, TypedStateValuesTwoDecompresses) {
  // Skip test if pointer compression is not enabled
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  const int numberOfInputs = 3;
  const ZoneVector<MachineType>* types =
      new (graph()->zone()->New(sizeof(ZoneVector<MachineType>)))
          ZoneVector<MachineType>(numberOfInputs, graph()->zone());
  SparseInputMask dense = SparseInputMask::Dense();
  const ElementAccess ElementAccesses[] = {
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::AnyTagged(),
       kNoWriteBarrier},
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::TaggedSigned(),
       kNoWriteBarrier},
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::TaggedPointer(),
       kNoWriteBarrier}};

  // For every access
  for (size_t i = 0; i < arraysize(ElementAccesses); ++i) {
    // Create the graph
    Node* load1 =
        graph()->NewNode(simplified()->LoadElement(ElementAccesses[i]), object,
                         index, effect, control);
    Node* changeToTagged1 = graph()->NewNode(
        machine()->ChangeCompressedPointerToTaggedPointer(), load1);
    Node* load2 =
        graph()->NewNode(simplified()->LoadElement(ElementAccesses[i]), object,
                         index, effect, control);
    Node* changeToTagged2 = graph()->NewNode(
        machine()->ChangeCompressedPointerToTaggedPointer(), load2);
    Node* typedStateValuesOneDecompress =
        graph()->NewNode(common()->TypedStateValues(types, dense),
                         changeToTagged1, load1, changeToTagged2);

    // Reduce
    StrictMock<MockAdvancedReducerEditor> editor;
    DecompressionElimination decompression_elimination(&editor, graph(),
                                                       machine(), common());
    Reduction r =
        decompression_elimination.Reduce(typedStateValuesOneDecompress);
    ASSERT_TRUE(r.Changed());
  }
}

TEST_F(DecompressionEliminationTest, TypedStateValuesAllDecompresses) {
  // Skip test if pointer compression is not enabled
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  const int numberOfInputs = 3;
  const ZoneVector<MachineType>* types =
      new (graph()->zone()->New(sizeof(ZoneVector<MachineType>)))
          ZoneVector<MachineType>(numberOfInputs, graph()->zone());
  SparseInputMask dense = SparseInputMask::Dense();
  const ElementAccess ElementAccesses[] = {
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::AnyTagged(),
       kNoWriteBarrier},
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::TaggedSigned(),
       kNoWriteBarrier},
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::TaggedPointer(),
       kNoWriteBarrier}};

  // For every access
  for (size_t i = 0; i < arraysize(ElementAccesses); ++i) {
    // Create the graph
    Node* load1 =
        graph()->NewNode(simplified()->LoadElement(ElementAccesses[i]), object,
                         index, effect, control);
    Node* changeToTagged1 = graph()->NewNode(
        machine()->ChangeCompressedPointerToTaggedPointer(), load1);
    Node* load2 =
        graph()->NewNode(simplified()->LoadElement(ElementAccesses[i]), object,
                         index, effect, control);
    Node* changeToTagged2 = graph()->NewNode(
        machine()->ChangeCompressedPointerToTaggedPointer(), load2);
    Node* load3 =
        graph()->NewNode(simplified()->LoadElement(ElementAccesses[i]), object,
                         index, effect, control);
    Node* changeToTagged3 = graph()->NewNode(
        machine()->ChangeCompressedPointerToTaggedPointer(), load3);
    Node* typedStateValuesOneDecompress =
        graph()->NewNode(common()->TypedStateValues(types, dense),
                         changeToTagged1, changeToTagged2, changeToTagged3);

    // Reduce
    StrictMock<MockAdvancedReducerEditor> editor;
    DecompressionElimination decompression_elimination(&editor, graph(),
                                                       machine(), common());
    Reduction r =
        decompression_elimination.Reduce(typedStateValuesOneDecompress);
    ASSERT_TRUE(r.Changed());
  }
}

TEST_F(DecompressionEliminationTest, TypedStateValuesNoDecompresses) {
  // Skip test if pointer compression is not enabled
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  const int numberOfInputs = 3;
  const ZoneVector<MachineType>* types =
      new (graph()->zone()->New(sizeof(ZoneVector<MachineType>)))
          ZoneVector<MachineType>(numberOfInputs, graph()->zone());
  SparseInputMask dense = SparseInputMask::Dense();
  const ElementAccess ElementAccesses[] = {
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::AnyTagged(),
       kNoWriteBarrier},
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::TaggedSigned(),
       kNoWriteBarrier},
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::TaggedPointer(),
       kNoWriteBarrier}};

  // For every access
  for (size_t i = 0; i < arraysize(ElementAccesses); ++i) {
    // Create the graph
    Node* load = graph()->NewNode(simplified()->LoadElement(ElementAccesses[i]),
                                  object, index, effect, control);
    Node* typedStateValuesOneDecompress = graph()->NewNode(
        common()->TypedStateValues(types, dense), load, load, load);

    // Reduce
    StrictMock<MockAdvancedReducerEditor> editor;
    DecompressionElimination decompression_elimination(&editor, graph(),
                                                       machine(), common());
    Reduction r =
        decompression_elimination.Reduce(typedStateValuesOneDecompress);
    ASSERT_FALSE(r.Changed());
  }
}

// -----------------------------------------------------------------------------
// Word64Equal comparison of two decompressions

TEST_F(DecompressionEliminationTest, TwoDecompressionWord64Equal) {
  // Skip test if pointer compression is not enabled
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);

  const Operator* DecompressionOps[] = {
      machine()->ChangeCompressedToTagged(),
      machine()->ChangeCompressedSignedToTaggedSigned(),
      machine()->ChangeCompressedPointerToTaggedPointer()};

  const ElementAccess ElementAccesses[] = {
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::AnyTagged(),
       kNoWriteBarrier},
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::TaggedSigned(),
       kNoWriteBarrier},
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::TaggedPointer(),
       kNoWriteBarrier}};

  ASSERT_EQ(arraysize(DecompressionOps), arraysize(ElementAccesses));

  // For every decompression (lhs)
  for (size_t j = 0; j < arraysize(DecompressionOps); ++j) {
    // For every decompression (rhs)
    for (size_t k = 0; k < arraysize(DecompressionOps); ++k) {
      // Create the graph
      Node* load1 =
          graph()->NewNode(simplified()->LoadElement(ElementAccesses[j]),
                           object, index, effect, control);
      Node* changeToTagged1 = graph()->NewNode(DecompressionOps[j], load1);
      Node* load2 =
          graph()->NewNode(simplified()->LoadElement(ElementAccesses[k]),
                           object, index, effect, control);
      Node* changeToTagged2 = graph()->NewNode(DecompressionOps[j], load2);
      Node* comparison = graph()->NewNode(machine()->Word64Equal(),
                                          changeToTagged1, changeToTagged2);
      // Reduce
      Reduction r = Reduce(comparison);
      ASSERT_TRUE(r.Changed());
      EXPECT_EQ(r.replacement()->opcode(), IrOpcode::kWord32Equal);
    }
  }
}

// -----------------------------------------------------------------------------
// Word64Equal comparison of two decompressions, where lhs == rhs

TEST_F(DecompressionEliminationTest, TwoDecompressionWord64EqualSameInput) {
  // Skip test if pointer compression is not enabled
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);

  const Operator* DecompressionOps[] = {
      machine()->ChangeCompressedToTagged(),
      machine()->ChangeCompressedSignedToTaggedSigned(),
      machine()->ChangeCompressedPointerToTaggedPointer()};

  const ElementAccess ElementAccesses[] = {
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::AnyTagged(),
       kNoWriteBarrier},
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::TaggedSigned(),
       kNoWriteBarrier},
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::TaggedPointer(),
       kNoWriteBarrier}};

  ASSERT_EQ(arraysize(DecompressionOps), arraysize(ElementAccesses));

  // For every decompression (same for lhs and rhs)
  for (size_t j = 0; j < arraysize(DecompressionOps); ++j) {
    // Create the graph
    Node* load = graph()->NewNode(simplified()->LoadElement(ElementAccesses[j]),
                                  object, index, effect, control);
    Node* changeToTagged = graph()->NewNode(DecompressionOps[j], load);
    Node* comparison = graph()->NewNode(machine()->Word64Equal(),
                                        changeToTagged, changeToTagged);
    // Reduce
    Reduction r = Reduce(comparison);
    ASSERT_TRUE(r.Changed());
    EXPECT_EQ(r.replacement()->opcode(), IrOpcode::kWord32Equal);
  }
}

// -----------------------------------------------------------------------------
// Word64Equal comparison of decompress and a constant

TEST_F(DecompressionEliminationTest, DecompressionConstantWord64Equal) {
  // Skip test if pointer compression is not enabled
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);

  const Operator* DecompressionOps[] = {
      machine()->ChangeCompressedToTagged(),
      machine()->ChangeCompressedSignedToTaggedSigned(),
      machine()->ChangeCompressedPointerToTaggedPointer()};

  const ElementAccess ElementAccesses[] = {
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::AnyTagged(),
       kNoWriteBarrier},
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::TaggedSigned(),
       kNoWriteBarrier},
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::TaggedPointer(),
       kNoWriteBarrier}};

  ASSERT_EQ(arraysize(DecompressionOps), arraysize(ElementAccesses));

  const int64_t constants[] = {static_cast<int64_t>(0x0000000000000000),
                               static_cast<int64_t>(0x0000000000000001),
                               static_cast<int64_t>(0x0000FFFFFFFF0000),
                               static_cast<int64_t>(0x7FFFFFFFFFFFFFFF),
                               static_cast<int64_t>(0x8000000000000000),
                               static_cast<int64_t>(0x8000000000000001),
                               static_cast<int64_t>(0x8000FFFFFFFF0000),
                               static_cast<int64_t>(0x8FFFFFFFFFFFFFFF),
                               static_cast<int64_t>(0xFFFFFFFFFFFFFFFF)};

  // For every decompression (lhs)
  for (size_t j = 0; j < arraysize(DecompressionOps); ++j) {
    // For every constant (rhs)
    for (size_t k = 0; k < arraysize(constants); ++k) {
      // Test with both (lhs, rhs) combinations
      for (bool lhsIsDecompression : {false, true}) {
        // Create the graph
        Node* load =
            graph()->NewNode(simplified()->LoadElement(ElementAccesses[j]),
                             object, index, effect, control);
        Node* changeToTagged = graph()->NewNode(DecompressionOps[j], load);
        Node* constant =
            graph()->NewNode(common()->Int64Constant(constants[k]));

        Node* lhs = lhsIsDecompression ? changeToTagged : constant;
        Node* rhs = lhsIsDecompression ? constant : changeToTagged;
        Node* comparison = graph()->NewNode(machine()->Word64Equal(), lhs, rhs);
        // Reduce
        Reduction r = Reduce(comparison);
        ASSERT_TRUE(r.Changed());
        EXPECT_EQ(r.replacement()->opcode(), IrOpcode::kWord32Equal);
      }
    }
  }
}

TEST_F(DecompressionEliminationTest, DecompressionHeapConstantWord64Equal) {
  // TODO(v8:8977): Disabling HeapConstant until CompressedHeapConstant
  // exists, since it breaks with verify CSA on.
  if (COMPRESS_POINTERS_BOOL) {
    return;
  }
  // Skip test if pointer compression is not enabled
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);

  const Operator* DecompressionOps[] = {
      machine()->ChangeCompressedToTagged(),
      machine()->ChangeCompressedSignedToTaggedSigned(),
      machine()->ChangeCompressedPointerToTaggedPointer()};

  const ElementAccess ElementAccesses[] = {
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::AnyTagged(),
       kNoWriteBarrier},
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::TaggedSigned(),
       kNoWriteBarrier},
      {kTaggedBase, kTaggedSize, Type::Any(), MachineType::TaggedPointer(),
       kNoWriteBarrier}};

  ASSERT_EQ(arraysize(DecompressionOps), arraysize(ElementAccesses));

  const Handle<HeapNumber> heapConstants[] = {
      factory()->NewHeapNumber(0.0),
      factory()->NewHeapNumber(-0.0),
      factory()->NewHeapNumber(11.2),
      factory()->NewHeapNumber(-11.2),
      factory()->NewHeapNumber(3.1415 + 1.4142),
      factory()->NewHeapNumber(3.1415 - 1.4142),
      factory()->NewHeapNumber(0x0000000000000000),
      factory()->NewHeapNumber(0x0000000000000001),
      factory()->NewHeapNumber(0x0000FFFFFFFF0000),
      factory()->NewHeapNumber(0x7FFFFFFFFFFFFFFF),
      factory()->NewHeapNumber(0x8000000000000000),
      factory()->NewHeapNumber(0x8000000000000001),
      factory()->NewHeapNumber(0x8000FFFFFFFF0000),
      factory()->NewHeapNumber(0x8FFFFFFFFFFFFFFF),
      factory()->NewHeapNumber(0xFFFFFFFFFFFFFFFF)};

  // For every decompression (lhs)
  for (size_t j = 0; j < arraysize(DecompressionOps); ++j) {
    // For every constant (rhs)
    for (size_t k = 0; k < arraysize(heapConstants); ++k) {
      // Test with both (lhs, rhs) combinations
      for (bool lhsIsDecompression : {false, true}) {
        // Create the graph
        Node* load =
            graph()->NewNode(simplified()->LoadElement(ElementAccesses[j]),
                             object, index, effect, control);
        Node* changeToTagged = graph()->NewNode(DecompressionOps[j], load);
        Node* constant =
            graph()->NewNode(common()->HeapConstant(heapConstants[k]));

        Node* lhs = lhsIsDecompression ? changeToTagged : constant;
        Node* rhs = lhsIsDecompression ? constant : changeToTagged;
        Node* comparison = graph()->NewNode(machine()->Word64Equal(), lhs, rhs);
        // Reduce
        Reduction r = Reduce(comparison);
        ASSERT_TRUE(r.Changed());
        EXPECT_EQ(r.replacement()->opcode(), IrOpcode::kWord32Equal);
      }
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
