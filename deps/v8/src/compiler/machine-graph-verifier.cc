// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/machine-graph-verifier.h"

#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/node.h"
#include "src/compiler/schedule.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

class MachineRepresentationInferrer {
 public:
  MachineRepresentationInferrer(Schedule const* schedule, Graph const* graph,
                                Linkage* linkage, Zone* zone)
      : schedule_(schedule),
        linkage_(linkage),
        representation_vector_(graph->NodeCount(), zone) {
    Run();
  }

  MachineRepresentation GetRepresentation(Node const* node) const {
    return representation_vector_.at(node->id());
  }

 private:
  MachineRepresentation GetProjectionType(Node const* projection) {
    size_t index = ProjectionIndexOf(projection->op());
    Node* input = projection->InputAt(0);
    switch (input->opcode()) {
      case IrOpcode::kInt32AddWithOverflow:
      case IrOpcode::kInt32SubWithOverflow:
      case IrOpcode::kInt32MulWithOverflow:
        CHECK_LE(index, static_cast<size_t>(1));
        return index == 0 ? MachineRepresentation::kWord32
                          : MachineRepresentation::kBit;
      case IrOpcode::kInt64AddWithOverflow:
      case IrOpcode::kInt64SubWithOverflow:
        CHECK_LE(index, static_cast<size_t>(1));
        return index == 0 ? MachineRepresentation::kWord64
                          : MachineRepresentation::kBit;
      case IrOpcode::kTryTruncateFloat32ToInt64:
      case IrOpcode::kTryTruncateFloat64ToInt64:
      case IrOpcode::kTryTruncateFloat32ToUint64:
      case IrOpcode::kTryTruncateFloat64ToUint64:
        CHECK_LE(index, static_cast<size_t>(1));
        return index == 0 ? MachineRepresentation::kWord64
                          : MachineRepresentation::kBit;
      case IrOpcode::kCall: {
        CallDescriptor const* desc = CallDescriptorOf(input->op());
        return desc->GetReturnType(index).representation();
      }
      default:
        return MachineRepresentation::kNone;
    }
  }

  void Run() {
    auto blocks = schedule_->all_blocks();
    for (BasicBlock* block : *blocks) {
      for (size_t i = 0; i <= block->NodeCount(); ++i) {
        Node const* node =
            i < block->NodeCount() ? block->NodeAt(i) : block->control_input();
        if (node == nullptr) {
          DCHECK_EQ(block->NodeCount(), i);
          break;
        }
        switch (node->opcode()) {
          case IrOpcode::kParameter:
            representation_vector_[node->id()] =
                linkage_->GetParameterType(ParameterIndexOf(node->op()))
                    .representation();
            break;
          case IrOpcode::kProjection: {
            representation_vector_[node->id()] = GetProjectionType(node);
          } break;
          case IrOpcode::kTypedStateValues:
            representation_vector_[node->id()] = MachineRepresentation::kNone;
            break;
          case IrOpcode::kAtomicLoad:
          case IrOpcode::kLoad:
          case IrOpcode::kProtectedLoad:
            representation_vector_[node->id()] =
                LoadRepresentationOf(node->op()).representation();
            break;
          case IrOpcode::kCheckedLoad:
            representation_vector_[node->id()] =
                CheckedLoadRepresentationOf(node->op()).representation();
            break;
          case IrOpcode::kLoadStackPointer:
          case IrOpcode::kLoadFramePointer:
          case IrOpcode::kLoadParentFramePointer:
            representation_vector_[node->id()] =
                MachineType::PointerRepresentation();
            break;
          case IrOpcode::kPhi:
            representation_vector_[node->id()] =
                PhiRepresentationOf(node->op());
            break;
          case IrOpcode::kCall: {
            CallDescriptor const* desc = CallDescriptorOf(node->op());
            if (desc->ReturnCount() > 0) {
              representation_vector_[node->id()] =
                  desc->GetReturnType(0).representation();
            } else {
              representation_vector_[node->id()] =
                  MachineRepresentation::kTagged;
            }
            break;
          }
          case IrOpcode::kUnalignedLoad:
            representation_vector_[node->id()] =
                UnalignedLoadRepresentationOf(node->op()).representation();
            break;
          case IrOpcode::kHeapConstant:
          case IrOpcode::kNumberConstant:
          case IrOpcode::kChangeBitToTagged:
          case IrOpcode::kIfException:
          case IrOpcode::kOsrValue:
          case IrOpcode::kChangeInt32ToTagged:
          case IrOpcode::kChangeUint32ToTagged:
          case IrOpcode::kBitcastWordToTagged:
            representation_vector_[node->id()] = MachineRepresentation::kTagged;
            break;
          case IrOpcode::kExternalConstant:
            representation_vector_[node->id()] =
                MachineType::PointerRepresentation();
            break;
          case IrOpcode::kBitcastTaggedToWord:
            representation_vector_[node->id()] =
                MachineType::PointerRepresentation();
            break;
          case IrOpcode::kBitcastWordToTaggedSigned:
            representation_vector_[node->id()] =
                MachineRepresentation::kTaggedSigned;
            break;
          case IrOpcode::kWord32Equal:
          case IrOpcode::kInt32LessThan:
          case IrOpcode::kInt32LessThanOrEqual:
          case IrOpcode::kUint32LessThan:
          case IrOpcode::kUint32LessThanOrEqual:
          case IrOpcode::kWord64Equal:
          case IrOpcode::kInt64LessThan:
          case IrOpcode::kInt64LessThanOrEqual:
          case IrOpcode::kUint64LessThan:
          case IrOpcode::kUint64LessThanOrEqual:
          case IrOpcode::kFloat32Equal:
          case IrOpcode::kFloat32LessThan:
          case IrOpcode::kFloat32LessThanOrEqual:
          case IrOpcode::kFloat64Equal:
          case IrOpcode::kFloat64LessThan:
          case IrOpcode::kFloat64LessThanOrEqual:
          case IrOpcode::kChangeTaggedToBit:
            representation_vector_[node->id()] = MachineRepresentation::kBit;
            break;
#define LABEL(opcode) case IrOpcode::k##opcode:
          case IrOpcode::kTruncateInt64ToInt32:
          case IrOpcode::kTruncateFloat32ToInt32:
          case IrOpcode::kTruncateFloat32ToUint32:
          case IrOpcode::kBitcastFloat32ToInt32:
          case IrOpcode::kInt32x4ExtractLane:
          case IrOpcode::kInt32Constant:
          case IrOpcode::kRelocatableInt32Constant:
          case IrOpcode::kTruncateFloat64ToWord32:
          case IrOpcode::kTruncateFloat64ToUint32:
          case IrOpcode::kChangeFloat64ToInt32:
          case IrOpcode::kChangeFloat64ToUint32:
          case IrOpcode::kRoundFloat64ToInt32:
          case IrOpcode::kFloat64ExtractLowWord32:
          case IrOpcode::kFloat64ExtractHighWord32:
            MACHINE_UNOP_32_LIST(LABEL)
            MACHINE_BINOP_32_LIST(LABEL) {
              representation_vector_[node->id()] =
                  MachineRepresentation::kWord32;
            }
            break;
          case IrOpcode::kChangeInt32ToInt64:
          case IrOpcode::kChangeUint32ToUint64:
          case IrOpcode::kInt64Constant:
          case IrOpcode::kRelocatableInt64Constant:
          case IrOpcode::kBitcastFloat64ToInt64:
            MACHINE_BINOP_64_LIST(LABEL) {
              representation_vector_[node->id()] =
                  MachineRepresentation::kWord64;
            }
            break;
          case IrOpcode::kRoundInt32ToFloat32:
          case IrOpcode::kRoundUint32ToFloat32:
          case IrOpcode::kRoundInt64ToFloat32:
          case IrOpcode::kRoundUint64ToFloat32:
          case IrOpcode::kFloat32Constant:
          case IrOpcode::kTruncateFloat64ToFloat32:
            MACHINE_FLOAT32_BINOP_LIST(LABEL)
            MACHINE_FLOAT32_UNOP_LIST(LABEL) {
              representation_vector_[node->id()] =
                  MachineRepresentation::kFloat32;
            }
            break;
          case IrOpcode::kRoundInt64ToFloat64:
          case IrOpcode::kRoundUint64ToFloat64:
          case IrOpcode::kChangeFloat32ToFloat64:
          case IrOpcode::kChangeInt32ToFloat64:
          case IrOpcode::kChangeUint32ToFloat64:
          case IrOpcode::kFloat64Constant:
          case IrOpcode::kFloat64SilenceNaN:
            MACHINE_FLOAT64_BINOP_LIST(LABEL)
            MACHINE_FLOAT64_UNOP_LIST(LABEL) {
              representation_vector_[node->id()] =
                  MachineRepresentation::kFloat64;
            }
            break;
#undef LABEL
          default:
            break;
        }
      }
    }
  }

  Schedule const* const schedule_;
  Linkage const* const linkage_;
  ZoneVector<MachineRepresentation> representation_vector_;
};

class MachineRepresentationChecker {
 public:
  MachineRepresentationChecker(Schedule const* const schedule,
                               MachineRepresentationInferrer const* const typer)
      : schedule_(schedule), typer_(typer) {}

  void Run() {
    BasicBlockVector const* blocks = schedule_->all_blocks();
    for (BasicBlock* block : *blocks) {
      for (size_t i = 0; i <= block->NodeCount(); ++i) {
        Node const* node =
            i < block->NodeCount() ? block->NodeAt(i) : block->control_input();
        if (node == nullptr) {
          DCHECK_EQ(block->NodeCount(), i);
          break;
        }
        switch (node->opcode()) {
          case IrOpcode::kCall:
          case IrOpcode::kTailCall:
            CheckCallInputs(node);
            break;
          case IrOpcode::kChangeBitToTagged:
            CHECK_EQ(MachineRepresentation::kBit,
                     typer_->GetRepresentation(node->InputAt(0)));
            break;
          case IrOpcode::kChangeTaggedToBit:
            CHECK_EQ(MachineRepresentation::kTagged,
                     typer_->GetRepresentation(node->InputAt(0)));
            break;
          case IrOpcode::kRoundInt64ToFloat64:
          case IrOpcode::kRoundUint64ToFloat64:
          case IrOpcode::kRoundInt64ToFloat32:
          case IrOpcode::kRoundUint64ToFloat32:
          case IrOpcode::kTruncateInt64ToInt32:
            CheckValueInputForInt64Op(node, 0);
            break;
          case IrOpcode::kBitcastWordToTagged:
          case IrOpcode::kBitcastWordToTaggedSigned:
            CheckValueInputRepresentationIs(
                node, 0, MachineType::PointerRepresentation());
            break;
          case IrOpcode::kBitcastTaggedToWord:
            CheckValueInputIsTagged(node, 0);
            break;
          case IrOpcode::kTruncateFloat64ToWord32:
          case IrOpcode::kTruncateFloat64ToUint32:
          case IrOpcode::kTruncateFloat64ToFloat32:
          case IrOpcode::kChangeFloat64ToInt32:
          case IrOpcode::kChangeFloat64ToUint32:
          case IrOpcode::kRoundFloat64ToInt32:
          case IrOpcode::kFloat64ExtractLowWord32:
          case IrOpcode::kFloat64ExtractHighWord32:
          case IrOpcode::kBitcastFloat64ToInt64:
            CheckValueInputForFloat64Op(node, 0);
            break;
          case IrOpcode::kWord64Equal:
            CheckValueInputIsTaggedOrPointer(node, 0);
            CheckValueInputRepresentationIs(
                node, 1, typer_->GetRepresentation(node->InputAt(0)));
            break;
          case IrOpcode::kInt64LessThan:
          case IrOpcode::kInt64LessThanOrEqual:
          case IrOpcode::kUint64LessThan:
          case IrOpcode::kUint64LessThanOrEqual:
            CheckValueInputForInt64Op(node, 0);
            CheckValueInputForInt64Op(node, 1);
            break;
          case IrOpcode::kInt32x4ExtractLane:
            CheckValueInputRepresentationIs(node, 0,
                                            MachineRepresentation::kSimd128);
            break;
#define LABEL(opcode) case IrOpcode::k##opcode:
          case IrOpcode::kChangeInt32ToTagged:
          case IrOpcode::kChangeUint32ToTagged:
          case IrOpcode::kChangeInt32ToFloat64:
          case IrOpcode::kChangeUint32ToFloat64:
          case IrOpcode::kRoundInt32ToFloat32:
          case IrOpcode::kRoundUint32ToFloat32:
          case IrOpcode::kChangeInt32ToInt64:
          case IrOpcode::kChangeUint32ToUint64:
            MACHINE_UNOP_32_LIST(LABEL) { CheckValueInputForInt32Op(node, 0); }
            break;
          case IrOpcode::kWord32Equal:
          case IrOpcode::kInt32LessThan:
          case IrOpcode::kInt32LessThanOrEqual:
          case IrOpcode::kUint32LessThan:
          case IrOpcode::kUint32LessThanOrEqual:
            MACHINE_BINOP_32_LIST(LABEL) {
              CheckValueInputForInt32Op(node, 0);
              CheckValueInputForInt32Op(node, 1);
            }
            break;
            MACHINE_BINOP_64_LIST(LABEL) {
              CheckValueInputForInt64Op(node, 0);
              CheckValueInputForInt64Op(node, 1);
            }
            break;
          case IrOpcode::kFloat32Equal:
          case IrOpcode::kFloat32LessThan:
          case IrOpcode::kFloat32LessThanOrEqual:
            MACHINE_FLOAT32_BINOP_LIST(LABEL) {
              CheckValueInputForFloat32Op(node, 0);
              CheckValueInputForFloat32Op(node, 1);
            }
            break;
          case IrOpcode::kChangeFloat32ToFloat64:
          case IrOpcode::kTruncateFloat32ToInt32:
          case IrOpcode::kTruncateFloat32ToUint32:
          case IrOpcode::kBitcastFloat32ToInt32:
            MACHINE_FLOAT32_UNOP_LIST(LABEL) {
              CheckValueInputForFloat32Op(node, 0);
            }
            break;
          case IrOpcode::kFloat64Equal:
          case IrOpcode::kFloat64LessThan:
          case IrOpcode::kFloat64LessThanOrEqual:
            MACHINE_FLOAT64_BINOP_LIST(LABEL) {
              CheckValueInputForFloat64Op(node, 0);
              CheckValueInputForFloat64Op(node, 1);
            }
            break;
          case IrOpcode::kFloat64SilenceNaN:
            MACHINE_FLOAT64_UNOP_LIST(LABEL) {
              CheckValueInputForFloat64Op(node, 0);
            }
            break;
#undef LABEL
          case IrOpcode::kParameter:
          case IrOpcode::kProjection:
            break;
          case IrOpcode::kLoad:
          case IrOpcode::kAtomicLoad:
            CheckValueInputIsTaggedOrPointer(node, 0);
            CheckValueInputRepresentationIs(
                node, 1, MachineType::PointerRepresentation());
            break;
          case IrOpcode::kStore:
            CheckValueInputIsTaggedOrPointer(node, 0);
            CheckValueInputRepresentationIs(
                node, 1, MachineType::PointerRepresentation());
            switch (StoreRepresentationOf(node->op()).representation()) {
              case MachineRepresentation::kTagged:
              case MachineRepresentation::kTaggedPointer:
              case MachineRepresentation::kTaggedSigned:
                CheckValueInputIsTagged(node, 2);
                break;
              default:
                CheckValueInputRepresentationIs(
                    node, 2,
                    StoreRepresentationOf(node->op()).representation());
            }
            break;
          case IrOpcode::kAtomicStore:
            CheckValueInputIsTaggedOrPointer(node, 0);
            CheckValueInputRepresentationIs(
                node, 1, MachineType::PointerRepresentation());
            switch (AtomicStoreRepresentationOf(node->op())) {
              case MachineRepresentation::kTagged:
              case MachineRepresentation::kTaggedPointer:
              case MachineRepresentation::kTaggedSigned:
                CheckValueInputIsTagged(node, 2);
                break;
              default:
                CheckValueInputRepresentationIs(
                    node, 2, AtomicStoreRepresentationOf(node->op()));
            }
            break;
          case IrOpcode::kPhi:
            switch (typer_->GetRepresentation(node)) {
              case MachineRepresentation::kTagged:
              case MachineRepresentation::kTaggedPointer:
              case MachineRepresentation::kTaggedSigned:
                for (int i = 0; i < node->op()->ValueInputCount(); ++i) {
                  CheckValueInputIsTagged(node, i);
                }
                break;
              default:
                for (int i = 0; i < node->op()->ValueInputCount(); ++i) {
                  CheckValueInputRepresentationIs(
                      node, i, typer_->GetRepresentation(node));
                }
                break;
            }
            break;
          case IrOpcode::kBranch:
          case IrOpcode::kSwitch:
            CheckValueInputForInt32Op(node, 0);
            break;
          case IrOpcode::kReturn:
            // TODO(epertoso): use the linkage to determine which tipe we
            // should have here.
            break;
          case IrOpcode::kTypedStateValues:
          case IrOpcode::kFrameState:
            break;
          default:
            if (node->op()->ValueInputCount() != 0) {
              std::stringstream str;
              str << "Node #" << node->id() << ":" << *node->op()
                  << " in the machine graph is not being checked.";
              FATAL(str.str().c_str());
            }
            break;
        }
      }
    }
  }

 private:
  void CheckValueInputRepresentationIs(Node const* node, int index,
                                       MachineRepresentation representation) {
    Node const* input = node->InputAt(index);
    if (typer_->GetRepresentation(input) != representation) {
      std::stringstream str;
      str << "TypeError: node #" << node->id() << ":" << *node->op()
          << " uses node #" << input->id() << ":" << *input->op()
          << " which doesn't have a " << MachineReprToString(representation)
          << " representation.";
      FATAL(str.str().c_str());
    }
  }

  void CheckValueInputIsTagged(Node const* node, int index) {
    Node const* input = node->InputAt(index);
    switch (typer_->GetRepresentation(input)) {
      case MachineRepresentation::kTagged:
      case MachineRepresentation::kTaggedPointer:
      case MachineRepresentation::kTaggedSigned:
        return;
      default:
        break;
    }
    std::ostringstream str;
    str << "TypeError: node #" << node->id() << ":" << *node->op()
        << " uses node #" << input->id() << ":" << *input->op()
        << " which doesn't have a tagged representation.";
    FATAL(str.str().c_str());
  }

  void CheckValueInputIsTaggedOrPointer(Node const* node, int index) {
    Node const* input = node->InputAt(index);
    switch (typer_->GetRepresentation(input)) {
      case MachineRepresentation::kTagged:
      case MachineRepresentation::kTaggedPointer:
      case MachineRepresentation::kTaggedSigned:
        return;
      default:
        break;
    }
    if (typer_->GetRepresentation(input) !=
        MachineType::PointerRepresentation()) {
      std::ostringstream str;
      str << "TypeError: node #" << node->id() << ":" << *node->op()
          << " uses node #" << input->id() << ":" << *input->op()
          << " which doesn't have a tagged or pointer representation.";
      FATAL(str.str().c_str());
    }
  }

  void CheckValueInputForInt32Op(Node const* node, int index) {
    Node const* input = node->InputAt(index);
    switch (typer_->GetRepresentation(input)) {
      case MachineRepresentation::kBit:
      case MachineRepresentation::kWord8:
      case MachineRepresentation::kWord16:
      case MachineRepresentation::kWord32:
        return;
      case MachineRepresentation::kNone: {
        std::ostringstream str;
        str << "TypeError: node #" << input->id() << ":" << *input->op()
            << " is untyped.";
        FATAL(str.str().c_str());
        break;
      }
      default:
        break;
    }
    std::ostringstream str;
    str << "TypeError: node #" << node->id() << ":" << *node->op()
        << " uses node #" << input->id() << ":" << *input->op()
        << " which doesn't have an int32-compatible representation.";
    FATAL(str.str().c_str());
  }

  void CheckValueInputForInt64Op(Node const* node, int index) {
    Node const* input = node->InputAt(index);
    switch (typer_->GetRepresentation(input)) {
      case MachineRepresentation::kWord64:
        return;
      case MachineRepresentation::kNone: {
        std::ostringstream str;
        str << "TypeError: node #" << input->id() << ":" << *input->op()
            << " is untyped.";
        FATAL(str.str().c_str());
        break;
      }

      default:
        break;
    }
    std::ostringstream str;
    str << "TypeError: node #" << node->id() << ":" << *node->op()
        << " uses node #" << input->id() << ":" << *input->op()
        << " which doesn't have a kWord64 representation.";
    FATAL(str.str().c_str());
  }

  void CheckValueInputForFloat32Op(Node const* node, int index) {
    Node const* input = node->InputAt(index);
    if (MachineRepresentation::kFloat32 == typer_->GetRepresentation(input)) {
      return;
    }
    std::ostringstream str;
    str << "TypeError: node #" << node->id() << ":" << *node->op()
        << " uses node #" << input->id() << ":" << *input->op()
        << " which doesn't have a kFloat32 representation.";
    FATAL(str.str().c_str());
  }

  void CheckValueInputForFloat64Op(Node const* node, int index) {
    Node const* input = node->InputAt(index);
    if (MachineRepresentation::kFloat64 == typer_->GetRepresentation(input)) {
      return;
    }
    std::ostringstream str;
    str << "TypeError: node #" << node->id() << ":" << *node->op()
        << " uses node #" << input->id() << ":" << *input->op()
        << " which doesn't have a kFloat64 representation.";
    FATAL(str.str().c_str());
  }

  void CheckCallInputs(Node const* node) {
    CallDescriptor const* desc = CallDescriptorOf(node->op());
    std::ostringstream str;
    bool should_log_error = false;
    for (size_t i = 0; i < desc->InputCount(); ++i) {
      Node const* input = node->InputAt(static_cast<int>(i));
      MachineRepresentation const input_type = typer_->GetRepresentation(input);
      MachineRepresentation const expected_input_type =
          desc->GetInputType(i).representation();
      if (!IsCompatible(expected_input_type, input_type)) {
        if (!should_log_error) {
          should_log_error = true;
          str << "TypeError: node #" << node->id() << ":" << *node->op()
              << " has wrong type for:" << std::endl;
        } else {
          str << std::endl;
        }
        str << " * input " << i << " (" << input->id() << ":" << *input->op()
            << ") doesn't have a " << MachineReprToString(expected_input_type)
            << " representation.";
      }
    }
    if (should_log_error) {
      FATAL(str.str().c_str());
    }
  }

  bool Intersect(MachineRepresentation lhs, MachineRepresentation rhs) {
    return (GetRepresentationProperties(lhs) &
            GetRepresentationProperties(rhs)) != 0;
  }

  enum RepresentationProperties { kIsPointer = 1, kIsTagged = 2 };

  int GetRepresentationProperties(MachineRepresentation representation) {
    switch (representation) {
      case MachineRepresentation::kTagged:
      case MachineRepresentation::kTaggedPointer:
        return kIsPointer | kIsTagged;
      case MachineRepresentation::kTaggedSigned:
        return kIsTagged;
      case MachineRepresentation::kWord32:
        return MachineRepresentation::kWord32 ==
                       MachineType::PointerRepresentation()
                   ? kIsPointer
                   : 0;
      case MachineRepresentation::kWord64:
        return MachineRepresentation::kWord64 ==
                       MachineType::PointerRepresentation()
                   ? kIsPointer
                   : 0;
      default:
        return 0;
    }
  }

  bool IsCompatible(MachineRepresentation expected,
                    MachineRepresentation actual) {
    switch (expected) {
      case MachineRepresentation::kTagged:
        return (actual == MachineRepresentation::kTagged ||
                actual == MachineRepresentation::kTaggedSigned ||
                actual == MachineRepresentation::kTaggedPointer);
      case MachineRepresentation::kTaggedSigned:
      case MachineRepresentation::kTaggedPointer:
      case MachineRepresentation::kFloat32:
      case MachineRepresentation::kFloat64:
      case MachineRepresentation::kSimd128:
      case MachineRepresentation::kBit:
      case MachineRepresentation::kWord8:
      case MachineRepresentation::kWord16:
      case MachineRepresentation::kWord64:
        return expected == actual;
        break;
      case MachineRepresentation::kWord32:
        return (actual == MachineRepresentation::kBit ||
                actual == MachineRepresentation::kWord8 ||
                actual == MachineRepresentation::kWord16 ||
                actual == MachineRepresentation::kWord32);
      case MachineRepresentation::kNone:
        UNREACHABLE();
    }
    return false;
  }

  Schedule const* const schedule_;
  MachineRepresentationInferrer const* const typer_;
};

}  // namespace

void MachineGraphVerifier::Run(Graph* graph, Schedule const* const schedule,
                               Linkage* linkage, Zone* temp_zone) {
  MachineRepresentationInferrer representation_inferrer(schedule, graph,
                                                        linkage, temp_zone);
  MachineRepresentationChecker checker(schedule, &representation_inferrer);
  checker.Run();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
