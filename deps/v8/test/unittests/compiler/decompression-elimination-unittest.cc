// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/decompression-elimination.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/simplified-operator.h"
#include "test/unittests/compiler/graph-reducer-unittest.h"
#include "test/unittests/compiler/graph-unittest.h"
#include "test/unittests/compiler/node-test-utils.h"
#include "testing/gmock-support.h"

using testing::_;
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
  Reduction Reduce(StrictMock<MockAdvancedReducerEditor>* editor, Node* node) {
    DecompressionElimination decompression_elimination(editor, graph(),
                                                       machine(), common());
    return decompression_elimination.Reduce(node);
  }
  Reduction Reduce(Node* node) {
    StrictMock<MockAdvancedReducerEditor> editor;
    return Reduce(&editor, node);
  }
  Node* GetUniqueValueUse(Node* node) {
    Node* value_use = nullptr;
    for (Edge edge : node->use_edges()) {
      if (NodeProperties::IsValueEdge(edge)) {
        if (value_use) {
          return nullptr;
        } else {
          value_use = edge.from();
        }
      }
    }
    // Return the value use of node after the reduction, if there is exactly one
    return value_use;
  }

  const Operator* DecompressionOpFromAccess(const ElementAccess access) {
    switch (access.machine_type.representation()) {
      case MachineRepresentation::kCompressed:
        return machine()->ChangeCompressedToTagged();
      case MachineRepresentation::kCompressedSigned:
        return machine()->ChangeCompressedSignedToTaggedSigned();
      case MachineRepresentation::kCompressedPointer:
        return machine()->ChangeCompressedPointerToTaggedPointer();
      default:
        UNREACHABLE();
    }
  }

  const Operator* CompressionOpFromAccess(const ElementAccess access) {
    switch (access.machine_type.representation()) {
      case MachineRepresentation::kCompressed:
        return machine()->ChangeTaggedToCompressed();
      case MachineRepresentation::kCompressedSigned:
        return machine()->ChangeTaggedSignedToCompressedSigned();
      case MachineRepresentation::kCompressedPointer:
        return machine()->ChangeTaggedPointerToCompressedPointer();
      default:
        UNREACHABLE();
    }
  }

  // 'Global' accesses used to simplify the tests.
  ElementAccess const any_access = {kTaggedBase, kTaggedSize, Type::Any(),
                                    MachineType::AnyCompressed(),
                                    kNoWriteBarrier};
  ElementAccess const signed_access = {kTaggedBase, kTaggedSize, Type::Any(),
                                       MachineType::CompressedSigned(),
                                       kNoWriteBarrier};
  ElementAccess const pointer_access = {kTaggedBase, kTaggedSize, Type::Any(),
                                        MachineType::CompressedPointer(),
                                        kNoWriteBarrier};
  const ElementAccess element_accesses[3] = {any_access, signed_access,
                                             pointer_access};

  MachineOperatorBuilder* machine() { return &machine_; }
  SimplifiedOperatorBuilder* simplified() { return &simplified_; }

 private:
  MachineOperatorBuilder machine_;
  SimplifiedOperatorBuilder simplified_;
};

// -----------------------------------------------------------------------------
// Direct Decompression & Compression.

TEST_F(DecompressionEliminationTest, BasicDecompressionCompression) {
  // Skip test if pointer compression is not enabled.
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);

  // Pairs of <load, store> accesses
  const std::pair<ElementAccess, ElementAccess> accesses[] = {
      {any_access, any_access},        {signed_access, any_access},
      {pointer_access, any_access},    {any_access, signed_access},
      {signed_access, signed_access},  {any_access, pointer_access},
      {pointer_access, pointer_access}};

  for (size_t i = 0; i < arraysize(accesses); ++i) {
    // Create the graph.
    Node* load = graph()->NewNode(simplified()->LoadElement(accesses[i].first),
                                  object, index, effect, control);
    Node* change_to_tagged =
        graph()->NewNode(DecompressionOpFromAccess(accesses[i].first), load);
    Node* change_to_compressed = graph()->NewNode(
        CompressionOpFromAccess(accesses[i].second), change_to_tagged);
    effect =
        graph()->NewNode(simplified()->StoreElement(accesses[i].second), object,
                         index, change_to_compressed, effect, control);

    // Reduce.
    Reduction r = Reduce(change_to_compressed);
    ASSERT_TRUE(r.Changed());
    EXPECT_EQ(load, r.replacement());
  }
}

// -----------------------------------------------------------------------------
// Direct Compression & Decompression

TEST_F(DecompressionEliminationTest, BasicCompressionDecompression) {
  // Skip test if pointer compression is not enabled.
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);

  // Pairs of <load, store> accesses
  const std::pair<ElementAccess, ElementAccess> accesses[] = {
      {any_access, any_access},        {signed_access, any_access},
      {pointer_access, any_access},    {any_access, signed_access},
      {signed_access, signed_access},  {any_access, pointer_access},
      {pointer_access, pointer_access}};

  for (size_t i = 0; i < arraysize(accesses); ++i) {
    // Create the graph.
    Node* load = graph()->NewNode(simplified()->LoadElement(accesses[i].first),
                                  object, index, effect, control);
    Node* change_to_compressed =
        graph()->NewNode(CompressionOpFromAccess(accesses[i].first), load);
    Node* change_to_tagged = graph()->NewNode(
        DecompressionOpFromAccess(accesses[i].second), change_to_compressed);
    effect = graph()->NewNode(simplified()->StoreElement(accesses[i].second),
                              object, index, change_to_tagged, effect, control);

    // Reduce.
    Reduction r = Reduce(change_to_tagged);
    ASSERT_TRUE(r.Changed());
    EXPECT_EQ(load, r.replacement());
  }
}

// -----------------------------------------------------------------------------
// Compress after constant.

TEST_F(DecompressionEliminationTest, CompressionAfterInt64Constant) {
  // Skip test if pointer compression is not enabled.
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);

  const int64_t constants[] = {static_cast<int64_t>(0x0000000000000000),
                               static_cast<int64_t>(0x0000000000000001),
                               static_cast<int64_t>(0x0000FFFFFFFF0000),
                               static_cast<int64_t>(0x7FFFFFFFFFFFFFFF),
                               static_cast<int64_t>(0x8000000000000000),
                               static_cast<int64_t>(0x8000000000000001),
                               static_cast<int64_t>(0x8000FFFFFFFF0000),
                               static_cast<int64_t>(0x8FFFFFFFFFFFFFFF),
                               static_cast<int64_t>(0xFFFFFFFFFFFFFFFF)};

  // For every access.
  for (size_t i = 0; i < arraysize(element_accesses); ++i) {
    // For every Int64Constant.
    for (size_t j = 0; j < arraysize(constants); ++j) {
      // Create the graph.
      Node* constant = graph()->NewNode(common()->Int64Constant(constants[j]));
      Node* change_to_compressed = graph()->NewNode(
          CompressionOpFromAccess(element_accesses[i]), constant);
      effect = graph()->NewNode(simplified()->StoreElement(element_accesses[i]),
                                object, index, change_to_compressed, effect,
                                control);
      // Reduce.
      Reduction r = Reduce(change_to_compressed);
      ASSERT_TRUE(r.Changed());
      EXPECT_EQ(r.replacement()->opcode(), IrOpcode::kInt32Constant);
    }
  }
}

TEST_F(DecompressionEliminationTest, CompressionAfterHeapConstant) {
  // Skip test if pointer compression is not enabled.
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);

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

  // For every access.
  for (size_t i = 0; i < arraysize(element_accesses); ++i) {
    // For every HeapNumber.
    for (size_t j = 0; j < arraysize(heap_constants); ++j) {
      // Create the graph.
      Node* constant =
          graph()->NewNode(common()->HeapConstant(heap_constants[j]));
      Node* change_to_compressed = graph()->NewNode(
          CompressionOpFromAccess(element_accesses[i]), constant);
      effect = graph()->NewNode(simplified()->StoreElement(element_accesses[i]),
                                object, index, change_to_compressed, effect,
                                control);
      // Reduce.
      Reduction r = Reduce(change_to_compressed);
      ASSERT_TRUE(r.Changed());
      EXPECT_EQ(r.replacement()->opcode(), IrOpcode::kCompressedHeapConstant);
    }
  }
}

// -----------------------------------------------------------------------------
// Phi.

TEST_F(DecompressionEliminationTest, PhiOneDecompress) {
  // Skip test if pointer compression is not enabled.
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  const int number_of_inputs = 1;

  // For every access.
  for (size_t i = 0; i < arraysize(element_accesses); ++i) {
    // Create the graph.
    Node* load =
        graph()->NewNode(simplified()->LoadElement(element_accesses[i]), object,
                         index, effect, control);
    Node* change_to_tagged =
        graph()->NewNode(DecompressionOpFromAccess(element_accesses[i]), load);
    Node* phi = graph()->NewNode(
        common()->Phi(MachineRepresentation::kTagged, number_of_inputs),
        change_to_tagged, control);

    // Reduce.
    StrictMock<MockAdvancedReducerEditor> editor;
    EXPECT_CALL(editor, ReplaceWithValue(phi, _, _, _));
    Reduction r = Reduce(&editor, phi);
    ASSERT_TRUE(r.Changed());

    // Get the actual decompress after the Phi, and check against the expected
    // one.
    Node* decompress = GetUniqueValueUse(phi);
    EXPECT_EQ(DecompressionOpFromAccess(element_accesses[i]), decompress->op());
  }
}

TEST_F(DecompressionEliminationTest, PhiThreeDecompressSameRepresentation) {
  // Skip test if pointer compression is not enabled.
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  const int number_of_inputs = 3;

  // For every access.
  for (size_t i = 0; i < arraysize(element_accesses); ++i) {
    // Create the graph.
    Node* load1 =
        graph()->NewNode(simplified()->LoadElement(element_accesses[i]), object,
                         index, effect, control);
    Node* load2 =
        graph()->NewNode(simplified()->LoadElement(element_accesses[i]), object,
                         index, effect, control);
    Node* load3 =
        graph()->NewNode(simplified()->LoadElement(element_accesses[i]), object,
                         index, effect, control);
    Node* change_to_tagged_1 =
        graph()->NewNode(DecompressionOpFromAccess(element_accesses[i]), load1);
    Node* change_to_tagged_2 =
        graph()->NewNode(DecompressionOpFromAccess(element_accesses[i]), load2);
    Node* change_to_tagged_3 =
        graph()->NewNode(DecompressionOpFromAccess(element_accesses[i]), load3);

    Node* phi = graph()->NewNode(
        common()->Phi(MachineRepresentation::kTagged, number_of_inputs),
        change_to_tagged_1, change_to_tagged_2, change_to_tagged_3, control);

    // Reduce.
    StrictMock<MockAdvancedReducerEditor> editor;
    EXPECT_CALL(editor, ReplaceWithValue(phi, _, _, _));
    Reduction r = Reduce(&editor, phi);
    ASSERT_TRUE(r.Changed());

    // Get the actual decompress after the Phi, and check against the expected
    // one.
    Node* decompress = GetUniqueValueUse(phi);
    EXPECT_EQ(DecompressionOpFromAccess(element_accesses[i]), decompress->op());
  }
}

TEST_F(DecompressionEliminationTest, PhiThreeDecompressOneAnyRepresentation) {
  // Skip test if pointer compression is not enabled.
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  const int number_of_inputs = 3;

  // Signed and Pointer (and not Any) accesses.
  const ElementAccess not_any_accesses[] = {signed_access, pointer_access};

  // For every access.
  for (size_t i = 0; i < arraysize(not_any_accesses); ++i) {
    // Create the graph.
    Node* load1 =
        graph()->NewNode(simplified()->LoadElement(not_any_accesses[i]), object,
                         index, effect, control);
    Node* load2 =
        graph()->NewNode(simplified()->LoadElement(not_any_accesses[i]), object,
                         index, effect, control);
    // Note that load3 loads a CompressedAny instead of not_any_accesses[i]
    Node* load3 = graph()->NewNode(simplified()->LoadElement(any_access),
                                   object, index, effect, control);
    Node* change_to_tagged_1 =
        graph()->NewNode(DecompressionOpFromAccess(not_any_accesses[i]), load1);
    Node* change_to_tagged_2 =
        graph()->NewNode(DecompressionOpFromAccess(not_any_accesses[i]), load2);
    Node* change_to_tagged_3 =
        graph()->NewNode(machine()->ChangeCompressedToTagged(), load3);

    Node* phi = graph()->NewNode(
        common()->Phi(MachineRepresentation::kTagged, number_of_inputs),
        change_to_tagged_1, change_to_tagged_2, change_to_tagged_3, control);

    // Reduce.
    StrictMock<MockAdvancedReducerEditor> editor;
    EXPECT_CALL(editor, ReplaceWithValue(phi, _, _, _));
    Reduction r = Reduce(&editor, phi);
    ASSERT_TRUE(r.Changed());

    // Get the actual decompress after the Phi, and check against the expected
    // one.
    Node* decompress = GetUniqueValueUse(phi);
    EXPECT_EQ(machine()->ChangeCompressedToTagged(), decompress->op());
  }
}

TEST_F(DecompressionEliminationTest, PhiThreeInputsOneNotDecompressed) {
  // Skip test if pointer compression is not enabled.
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  const int number_of_inputs = 3;

  // For every access.
  for (size_t i = 0; i < arraysize(element_accesses); ++i) {
    // Create the graph.
    Node* load1 =
        graph()->NewNode(simplified()->LoadElement(element_accesses[i]), object,
                         index, effect, control);
    Node* load2 =
        graph()->NewNode(simplified()->LoadElement(element_accesses[i]), object,
                         index, effect, control);
    Node* load3 =
        graph()->NewNode(simplified()->LoadElement(element_accesses[i]), object,
                         index, effect, control);
    Node* change_to_tagged_1 =
        graph()->NewNode(DecompressionOpFromAccess(element_accesses[i]), load1);
    Node* change_to_tagged_2 =
        graph()->NewNode(DecompressionOpFromAccess(element_accesses[i]), load2);

    Node* phi = graph()->NewNode(
        common()->Phi(MachineRepresentation::kTagged, number_of_inputs),
        change_to_tagged_1, change_to_tagged_2, load3, control);

    // Reduce.
    Reduction r = Reduce(phi);
    ASSERT_FALSE(r.Changed());
  }
}

// In the case of having one decompress Signed and one Pointer, we have to
// generate the conservative decompress any after the Phi.
TEST_F(DecompressionEliminationTest, PhiTwoDecompressesOneSignedOnePointer) {
  // Skip test if pointer compression is not enabled.
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  const int number_of_inputs = 2;

  // Create the graph.
  Node* load1 = graph()->NewNode(simplified()->LoadElement(signed_access),
                                 object, index, effect, control);
  Node* load2 = graph()->NewNode(simplified()->LoadElement(pointer_access),
                                 object, index, effect, control);
  Node* change_to_tagged_1 =
      graph()->NewNode(DecompressionOpFromAccess(signed_access), load1);
  Node* change_to_tagged_2 =
      graph()->NewNode(DecompressionOpFromAccess(pointer_access), load2);

  Node* phi = graph()->NewNode(
      common()->Phi(MachineRepresentation::kTagged, number_of_inputs),
      change_to_tagged_1, change_to_tagged_2, control);

  // Reduce.
  StrictMock<MockAdvancedReducerEditor> editor;
  EXPECT_CALL(editor, ReplaceWithValue(phi, _, _, _));
  Reduction r = Reduce(&editor, phi);
  ASSERT_TRUE(r.Changed());

  // Get the actual decompress after the Phi, and check against the expected
  // one.
  Node* decompress = GetUniqueValueUse(phi);
  EXPECT_EQ(machine()->ChangeCompressedToTagged(), decompress->op());
}

// -----------------------------------------------------------------------------
// TypedStateValues.

TEST_F(DecompressionEliminationTest, TypedStateValuesOneDecompress) {
  // Skip test if pointer compression is not enabled.
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  const int number_of_inputs = 1;
  const ZoneVector<MachineType>* types =
      new (graph()->zone()->New(sizeof(ZoneVector<MachineType>)))
          ZoneVector<MachineType>(number_of_inputs, graph()->zone());
  SparseInputMask dense = SparseInputMask::Dense();

  // For every access.
  for (size_t i = 0; i < arraysize(element_accesses); ++i) {
    // Create the graph.
    Node* load =
        graph()->NewNode(simplified()->LoadElement(element_accesses[i]), object,
                         index, effect, control);
    Node* change_to_tagged =
        graph()->NewNode(DecompressionOpFromAccess(element_accesses[i]), load);
    Node* typedStateValues = graph()->NewNode(
        common()->TypedStateValues(types, dense), change_to_tagged);

    // Reduce.
    StrictMock<MockAdvancedReducerEditor> editor;
    DecompressionElimination decompression_elimination(&editor, graph(),
                                                       machine(), common());
    Reduction r = decompression_elimination.Reduce(typedStateValues);
    ASSERT_TRUE(r.Changed());
    EXPECT_EQ(r.replacement()->InputAt(0), load);
  }
}

TEST_F(DecompressionEliminationTest, TypedStateValuesTwoDecompresses) {
  // Skip test if pointer compression is not enabled.
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  const int number_of_inputs = 3;
  const ZoneVector<MachineType>* types =
      new (graph()->zone()->New(sizeof(ZoneVector<MachineType>)))
          ZoneVector<MachineType>(number_of_inputs, graph()->zone());
  SparseInputMask dense = SparseInputMask::Dense();

  // For every access.
  for (size_t i = 0; i < arraysize(element_accesses); ++i) {
    // Create the graph.
    Node* load1 =
        graph()->NewNode(simplified()->LoadElement(element_accesses[i]), object,
                         index, effect, control);
    Node* change_to_tagged_1 =
        graph()->NewNode(DecompressionOpFromAccess(element_accesses[i]), load1);
    Node* load2 =
        graph()->NewNode(simplified()->LoadElement(element_accesses[i]), object,
                         index, effect, control);
    Node* change_to_tagged_2 =
        graph()->NewNode(DecompressionOpFromAccess(element_accesses[i]), load2);
    Node* typedStateValues =
        graph()->NewNode(common()->TypedStateValues(types, dense),
                         change_to_tagged_1, load1, change_to_tagged_2);

    // Reduce.
    StrictMock<MockAdvancedReducerEditor> editor;
    DecompressionElimination decompression_elimination(&editor, graph(),
                                                       machine(), common());
    Reduction r = decompression_elimination.Reduce(typedStateValues);
    ASSERT_TRUE(r.Changed());
    EXPECT_EQ(r.replacement()->InputAt(0), load1);
    // Note that the input at index 1 didn't change.
    EXPECT_EQ(r.replacement()->InputAt(1), load1);
    EXPECT_EQ(r.replacement()->InputAt(2), load2);
  }
}

TEST_F(DecompressionEliminationTest, TypedStateValuesAllDecompresses) {
  // Skip test if pointer compression is not enabled.
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  const int number_of_inputs = 3;
  const ZoneVector<MachineType>* types =
      new (graph()->zone()->New(sizeof(ZoneVector<MachineType>)))
          ZoneVector<MachineType>(number_of_inputs, graph()->zone());
  SparseInputMask dense = SparseInputMask::Dense();

  // For every access.
  for (size_t i = 0; i < arraysize(element_accesses); ++i) {
    // Create the graph.
    Node* load1 =
        graph()->NewNode(simplified()->LoadElement(element_accesses[i]), object,
                         index, effect, control);
    Node* change_to_tagged_1 =
        graph()->NewNode(DecompressionOpFromAccess(element_accesses[i]), load1);
    Node* load2 =
        graph()->NewNode(simplified()->LoadElement(element_accesses[i]), object,
                         index, effect, control);
    Node* change_to_tagged_2 =
        graph()->NewNode(DecompressionOpFromAccess(element_accesses[i]), load2);
    Node* load3 =
        graph()->NewNode(simplified()->LoadElement(element_accesses[i]), object,
                         index, effect, control);
    Node* change_to_tagged_3 =
        graph()->NewNode(DecompressionOpFromAccess(element_accesses[i]), load3);
    Node* typedStateValues = graph()->NewNode(
        common()->TypedStateValues(types, dense), change_to_tagged_1,
        change_to_tagged_2, change_to_tagged_3);

    // Reduce.
    StrictMock<MockAdvancedReducerEditor> editor;
    DecompressionElimination decompression_elimination(&editor, graph(),
                                                       machine(), common());
    Reduction r = decompression_elimination.Reduce(typedStateValues);
    ASSERT_TRUE(r.Changed());
    EXPECT_EQ(r.replacement()->InputAt(0), load1);
    EXPECT_EQ(r.replacement()->InputAt(1), load2);
    EXPECT_EQ(r.replacement()->InputAt(2), load3);
  }
}

TEST_F(DecompressionEliminationTest, TypedStateValuesNoDecompresses) {
  // Skip test if pointer compression is not enabled.
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);
  const int number_of_inputs = 3;
  const ZoneVector<MachineType>* types =
      new (graph()->zone()->New(sizeof(ZoneVector<MachineType>)))
          ZoneVector<MachineType>(number_of_inputs, graph()->zone());
  SparseInputMask dense = SparseInputMask::Dense();

  // For every access.
  for (size_t i = 0; i < arraysize(element_accesses); ++i) {
    // Create the graph.
    Node* load =
        graph()->NewNode(simplified()->LoadElement(element_accesses[i]), object,
                         index, effect, control);
    Node* typedStateValues = graph()->NewNode(
        common()->TypedStateValues(types, dense), load, load, load);

    // Reduce.
    StrictMock<MockAdvancedReducerEditor> editor;
    DecompressionElimination decompression_elimination(&editor, graph(),
                                                       machine(), common());
    Reduction r = decompression_elimination.Reduce(typedStateValues);
    ASSERT_FALSE(r.Changed());
  }
}

// -----------------------------------------------------------------------------
// Word64Equal comparison of two decompressions.

TEST_F(DecompressionEliminationTest, TwoDecompressionWord64Equal) {
  // Skip test if pointer compression is not enabled.
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);

  // For every decompression (lhs).
  for (size_t i = 0; i < arraysize(element_accesses); ++i) {
    // For every decompression (rhs)
    for (size_t j = 0; j < arraysize(element_accesses); ++j) {
      // Create the graph.
      Node* load1 =
          graph()->NewNode(simplified()->LoadElement(element_accesses[i]),
                           object, index, effect, control);
      Node* change_to_tagged_1 = graph()->NewNode(
          DecompressionOpFromAccess(element_accesses[i]), load1);
      Node* load2 =
          graph()->NewNode(simplified()->LoadElement(element_accesses[j]),
                           object, index, effect, control);
      Node* change_to_tagged_2 = graph()->NewNode(
          DecompressionOpFromAccess(element_accesses[i]), load2);
      Node* comparison = graph()->NewNode(
          machine()->Word64Equal(), change_to_tagged_1, change_to_tagged_2);
      // Reduce.
      Reduction r = Reduce(comparison);
      ASSERT_TRUE(r.Changed());
      EXPECT_EQ(r.replacement()->opcode(), IrOpcode::kWord32Equal);
    }
  }
}

// -----------------------------------------------------------------------------
// Word64Equal comparison of two decompressions, where lhs == rhs.

TEST_F(DecompressionEliminationTest, TwoDecompressionWord64EqualSameInput) {
  // Skip test if pointer compression is not enabled.
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);

  // For every access. (same for lhs and rhs)
  for (size_t i = 0; i < arraysize(element_accesses); ++i) {
    // Create the graph.
    Node* load =
        graph()->NewNode(simplified()->LoadElement(element_accesses[i]), object,
                         index, effect, control);
    Node* change_to_tagged =
        graph()->NewNode(DecompressionOpFromAccess(element_accesses[i]), load);
    Node* comparison = graph()->NewNode(machine()->Word64Equal(),
                                        change_to_tagged, change_to_tagged);
    // Reduce.
    Reduction r = Reduce(comparison);
    ASSERT_TRUE(r.Changed());
    EXPECT_EQ(r.replacement()->opcode(), IrOpcode::kWord32Equal);
  }
}

// -----------------------------------------------------------------------------
// Word64Equal comparison of decompress and a constant.

TEST_F(DecompressionEliminationTest, DecompressionConstantWord64Equal) {
  // Skip test if pointer compression is not enabled.
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);

  const int64_t constants[] = {static_cast<int64_t>(0x0000000000000000),
                               static_cast<int64_t>(0x0000000000000001),
                               static_cast<int64_t>(0x0000FFFFFFFF0000),
                               static_cast<int64_t>(0x7FFFFFFFFFFFFFFF),
                               static_cast<int64_t>(0x8000000000000000),
                               static_cast<int64_t>(0x8000000000000001),
                               static_cast<int64_t>(0x8000FFFFFFFF0000),
                               static_cast<int64_t>(0x8FFFFFFFFFFFFFFF),
                               static_cast<int64_t>(0xFFFFFFFFFFFFFFFF)};

  // For every decompression (lhs).
  for (size_t i = 0; i < arraysize(element_accesses); ++i) {
    // For every constant (rhs).
    for (size_t j = 0; j < arraysize(constants); ++j) {
      // Test with both (lhs, rhs) combinations.
      for (bool lhs_is_decompression : {false, true}) {
        // Create the graph.
        Node* load =
            graph()->NewNode(simplified()->LoadElement(element_accesses[i]),
                             object, index, effect, control);
        Node* change_to_tagged = graph()->NewNode(
            DecompressionOpFromAccess(element_accesses[i]), load);
        Node* constant =
            graph()->NewNode(common()->Int64Constant(constants[j]));

        Node* lhs = lhs_is_decompression ? change_to_tagged : constant;
        Node* rhs = lhs_is_decompression ? constant : change_to_tagged;
        Node* comparison = graph()->NewNode(machine()->Word64Equal(), lhs, rhs);
        // Reduce.
        Reduction r = Reduce(comparison);
        ASSERT_TRUE(r.Changed());
        EXPECT_EQ(r.replacement()->opcode(), IrOpcode::kWord32Equal);
      }
    }
  }
}

TEST_F(DecompressionEliminationTest, DecompressionHeapConstantWord64Equal) {
  // Skip test if pointer compression is not enabled.
  if (!COMPRESS_POINTERS_BOOL) {
    return;
  }

  // Define variables.
  Node* const control = graph()->start();
  Node* object = Parameter(Type::Any(), 0);
  Node* effect = graph()->start();
  Node* index = Parameter(Type::UnsignedSmall(), 1);

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

  // For every decompression (lhs).
  for (size_t i = 0; i < arraysize(element_accesses); ++i) {
    // For every constant (rhs).
    for (size_t j = 0; j < arraysize(heap_constants); ++j) {
      // Test with both (lhs, rhs) combinations.
      for (bool lhs_is_decompression : {false, true}) {
        // Create the graph.
        Node* load =
            graph()->NewNode(simplified()->LoadElement(element_accesses[i]),
                             object, index, effect, control);
        Node* change_to_tagged = graph()->NewNode(
            DecompressionOpFromAccess(element_accesses[i]), load);
        Node* constant =
            graph()->NewNode(common()->HeapConstant(heap_constants[j]));

        Node* lhs = lhs_is_decompression ? change_to_tagged : constant;
        Node* rhs = lhs_is_decompression ? constant : change_to_tagged;
        Node* comparison = graph()->NewNode(machine()->Word64Equal(), lhs, rhs);
        // Reduce.
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
