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
        representation_vector_(graph->NodeCount(), MachineRepresentation::kNone,
                               zone) {
    Run();
  }

  CallDescriptor* call_descriptor() const {
    return linkage_->GetIncomingDescriptor();
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
        CHECK_LE(index, static_cast<size_t>(1));
        return index == 0 ? MachineRepresentation::kWord64
                          : MachineRepresentation::kBit;
      case IrOpcode::kCall:
      case IrOpcode::kCallWithCallerSavedRegisters: {
        CallDescriptor const* desc = CallDescriptorOf(input->op());
        return desc->GetReturnType(index).representation();
      }
      default:
        return MachineRepresentation::kNone;
    }
  }

  MachineRepresentation PromoteRepresentation(MachineRepresentation rep) {
    switch (rep) {
      case MachineRepresentation::kWord8:
      case MachineRepresentation::kWord16:
      case MachineRepresentation::kWord32:
        return MachineRepresentation::kWord32;
      default:
        break;
    }
    return rep;
  }

  void Run() {
    auto blocks = schedule_->all_blocks();
    for (BasicBlock* block : *blocks) {
      current_block_ = block;
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
          case IrOpcode::kReturn: {
            representation_vector_[node->id()] = PromoteRepresentation(
                linkage_->GetReturnType().representation());
            break;
          }
          case IrOpcode::kProjection: {
            representation_vector_[node->id()] = GetProjectionType(node);
          } break;
          case IrOpcode::kTypedStateValues:
            representation_vector_[node->id()] = MachineRepresentation::kNone;
            break;
          case IrOpcode::kAtomicLoad:
          case IrOpcode::kLoad:
          case IrOpcode::kProtectedLoad:
            representation_vector_[node->id()] = PromoteRepresentation(
                LoadRepresentationOf(node->op()).representation());
            break;
          case IrOpcode::kCheckedLoad:
            representation_vector_[node->id()] = PromoteRepresentation(
                CheckedLoadRepresentationOf(node->op()).representation());
            break;
          case IrOpcode::kLoadStackPointer:
          case IrOpcode::kLoadFramePointer:
          case IrOpcode::kLoadParentFramePointer:
            representation_vector_[node->id()] =
                MachineType::PointerRepresentation();
            break;
          case IrOpcode::kUnalignedLoad:
            representation_vector_[node->id()] = PromoteRepresentation(
                UnalignedLoadRepresentationOf(node->op()).representation());
            break;
          case IrOpcode::kPhi:
            representation_vector_[node->id()] =
                PhiRepresentationOf(node->op());
            break;
          case IrOpcode::kCall:
          case IrOpcode::kCallWithCallerSavedRegisters: {
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
          case IrOpcode::kAtomicStore:
            representation_vector_[node->id()] =
                PromoteRepresentation(AtomicStoreRepresentationOf(node->op()));
            break;
          case IrOpcode::kAtomicExchange:
          case IrOpcode::kAtomicCompareExchange:
          case IrOpcode::kAtomicAdd:
          case IrOpcode::kAtomicSub:
          case IrOpcode::kAtomicAnd:
          case IrOpcode::kAtomicOr:
          case IrOpcode::kAtomicXor:
            representation_vector_[node->id()] = PromoteRepresentation(
                AtomicOpRepresentationOf(node->op()).representation());
            break;
          case IrOpcode::kStore:
          case IrOpcode::kProtectedStore:
            representation_vector_[node->id()] = PromoteRepresentation(
                StoreRepresentationOf(node->op()).representation());
            break;
          case IrOpcode::kCheckedStore:
            representation_vector_[node->id()] =
                PromoteRepresentation(CheckedStoreRepresentationOf(node->op()));
            break;
          case IrOpcode::kUnalignedStore:
            representation_vector_[node->id()] = PromoteRepresentation(
                UnalignedStoreRepresentationOf(node->op()));
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
          case IrOpcode::kI32x4ExtractLane:
          case IrOpcode::kI16x8ExtractLane:
          case IrOpcode::kI8x16ExtractLane:
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
          case IrOpcode::kChangeFloat64ToUint64:
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
  BasicBlock* current_block_;
};

class MachineRepresentationChecker {
 public:
  MachineRepresentationChecker(
      Schedule const* const schedule,
      MachineRepresentationInferrer const* const inferrer, bool is_stub,
      const char* name)
      : schedule_(schedule),
        inferrer_(inferrer),
        is_stub_(is_stub),
        name_(name),
        current_block_(nullptr) {}

  void Run() {
    BasicBlockVector const* blocks = schedule_->all_blocks();
    for (BasicBlock* block : *blocks) {
      current_block_ = block;
      for (size_t i = 0; i <= block->NodeCount(); ++i) {
        Node const* node =
            i < block->NodeCount() ? block->NodeAt(i) : block->control_input();
        if (node == nullptr) {
          DCHECK_EQ(block->NodeCount(), i);
          break;
        }
        switch (node->opcode()) {
          case IrOpcode::kCall:
          case IrOpcode::kCallWithCallerSavedRegisters:
          case IrOpcode::kTailCall:
            CheckCallInputs(node);
            break;
          case IrOpcode::kChangeBitToTagged:
            CHECK_EQ(MachineRepresentation::kBit,
                     inferrer_->GetRepresentation(node->InputAt(0)));
            break;
          case IrOpcode::kChangeTaggedToBit:
            CHECK_EQ(MachineRepresentation::kTagged,
                     inferrer_->GetRepresentation(node->InputAt(0)));
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
          case IrOpcode::kTryTruncateFloat64ToInt64:
            CheckValueInputForFloat64Op(node, 0);
            break;
          case IrOpcode::kWord64Equal:
            if (Is64()) {
              CheckValueInputIsTaggedOrPointer(node, 0);
              CheckValueInputIsTaggedOrPointer(node, 1);
              if (!is_stub_) {
                CheckValueInputRepresentationIs(
                    node, 1, inferrer_->GetRepresentation(node->InputAt(0)));
              }
            } else {
              CheckValueInputForInt64Op(node, 0);
              CheckValueInputForInt64Op(node, 1);
            }
            break;
          case IrOpcode::kInt64LessThan:
          case IrOpcode::kInt64LessThanOrEqual:
          case IrOpcode::kUint64LessThan:
          case IrOpcode::kUint64LessThanOrEqual:
            CheckValueInputForInt64Op(node, 0);
            CheckValueInputForInt64Op(node, 1);
            break;
          case IrOpcode::kI32x4ExtractLane:
          case IrOpcode::kI16x8ExtractLane:
          case IrOpcode::kI8x16ExtractLane:
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
            if (Is32()) {
              CheckValueInputIsTaggedOrPointer(node, 0);
              CheckValueInputIsTaggedOrPointer(node, 1);
              if (!is_stub_) {
                CheckValueInputRepresentationIs(
                    node, 1, inferrer_->GetRepresentation(node->InputAt(0)));
              }
            } else {
              CheckValueInputForInt32Op(node, 0);
              CheckValueInputForInt32Op(node, 1);
            }
            break;

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
          case IrOpcode::kChangeFloat64ToUint64:
            MACHINE_FLOAT64_UNOP_LIST(LABEL) {
              CheckValueInputForFloat64Op(node, 0);
            }
            break;
#undef LABEL
          case IrOpcode::kParameter:
          case IrOpcode::kProjection:
            break;
          case IrOpcode::kDebugAbort:
            CheckValueInputIsTagged(node, 0);
            break;
          case IrOpcode::kLoad:
          case IrOpcode::kAtomicLoad:
            CheckValueInputIsTaggedOrPointer(node, 0);
            CheckValueInputRepresentationIs(
                node, 1, MachineType::PointerRepresentation());
            break;
          case IrOpcode::kStore:
          case IrOpcode::kAtomicStore:
          case IrOpcode::kAtomicExchange:
          case IrOpcode::kAtomicAdd:
          case IrOpcode::kAtomicSub:
          case IrOpcode::kAtomicAnd:
          case IrOpcode::kAtomicOr:
          case IrOpcode::kAtomicXor:
            CheckValueInputIsTaggedOrPointer(node, 0);
            CheckValueInputRepresentationIs(
                node, 1, MachineType::PointerRepresentation());
            switch (inferrer_->GetRepresentation(node)) {
              case MachineRepresentation::kTagged:
              case MachineRepresentation::kTaggedPointer:
              case MachineRepresentation::kTaggedSigned:
                CheckValueInputIsTagged(node, 2);
                break;
              default:
                CheckValueInputRepresentationIs(
                    node, 2, inferrer_->GetRepresentation(node));
            }
            break;
          case IrOpcode::kAtomicCompareExchange:
            CheckValueInputIsTaggedOrPointer(node, 0);
            CheckValueInputRepresentationIs(
                node, 1, MachineType::PointerRepresentation());
            switch (inferrer_->GetRepresentation(node)) {
              case MachineRepresentation::kTagged:
              case MachineRepresentation::kTaggedPointer:
              case MachineRepresentation::kTaggedSigned:
                CheckValueInputIsTagged(node, 2);
                CheckValueInputIsTagged(node, 3);
                break;
              default:
                CheckValueInputRepresentationIs(
                    node, 2, inferrer_->GetRepresentation(node));
                CheckValueInputRepresentationIs(
                    node, 3, inferrer_->GetRepresentation(node));
            }
            break;
          case IrOpcode::kPhi:
            switch (inferrer_->GetRepresentation(node)) {
              case MachineRepresentation::kTagged:
              case MachineRepresentation::kTaggedPointer:
              case MachineRepresentation::kTaggedSigned:
                for (int i = 0; i < node->op()->ValueInputCount(); ++i) {
                  CheckValueInputIsTagged(node, i);
                }
                break;
              case MachineRepresentation::kWord32:
                for (int i = 0; i < node->op()->ValueInputCount(); ++i) {
                  CheckValueInputForInt32Op(node, i);
                }
                break;
              default:
                for (int i = 0; i < node->op()->ValueInputCount(); ++i) {
                  CheckValueInputRepresentationIs(
                      node, i, inferrer_->GetRepresentation(node));
                }
                break;
            }
            break;
          case IrOpcode::kBranch:
          case IrOpcode::kSwitch:
            CheckValueInputForInt32Op(node, 0);
            break;
          case IrOpcode::kReturn: {
            // TODO(ishell): enable once the pop count parameter type becomes
            // MachineType::PointerRepresentation(). Currently it's int32 or
            // word-size.
            // CheckValueInputRepresentationIs(
            //     node, 0, MachineType::PointerRepresentation());  // Pop count
            size_t return_count = inferrer_->call_descriptor()->ReturnCount();
            for (size_t i = 0; i < return_count; i++) {
              MachineType type = inferrer_->call_descriptor()->GetReturnType(i);
              int input_index = static_cast<int>(i + 1);
              switch (type.representation()) {
                case MachineRepresentation::kTagged:
                case MachineRepresentation::kTaggedPointer:
                case MachineRepresentation::kTaggedSigned:
                  CheckValueInputIsTagged(node, input_index);
                  break;
                case MachineRepresentation::kWord32:
                  CheckValueInputForInt32Op(node, input_index);
                  break;
                default:
                  CheckValueInputRepresentationIs(node, input_index,
                                                  type.representation());
                  break;
              }
            }
            break;
          }
          case IrOpcode::kThrow:
          case IrOpcode::kTypedStateValues:
          case IrOpcode::kFrameState:
            break;
          default:
            if (node->op()->ValueInputCount() != 0) {
              std::stringstream str;
              str << "Node #" << node->id() << ":" << *node->op()
                  << " in the machine graph is not being checked.";
              PrintDebugHelp(str, node);
              FATAL(str.str().c_str());
            }
            break;
        }
      }
    }
  }

 private:
  static bool Is32() {
    return MachineType::PointerRepresentation() ==
           MachineRepresentation::kWord32;
  }
  static bool Is64() {
    return MachineType::PointerRepresentation() ==
           MachineRepresentation::kWord64;
  }

  void CheckValueInputRepresentationIs(Node const* node, int index,
                                       MachineRepresentation representation) {
    Node const* input = node->InputAt(index);
    MachineRepresentation input_representation =
        inferrer_->GetRepresentation(input);
    if (input_representation != representation) {
      std::stringstream str;
      str << "TypeError: node #" << node->id() << ":" << *node->op()
          << " uses node #" << input->id() << ":" << *input->op() << ":"
          << input_representation << " which doesn't have a " << representation
          << " representation.";
      PrintDebugHelp(str, node);
      FATAL(str.str().c_str());
    }
  }

  void CheckValueInputIsTagged(Node const* node, int index) {
    Node const* input = node->InputAt(index);
    switch (inferrer_->GetRepresentation(input)) {
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
    PrintDebugHelp(str, node);
    FATAL(str.str().c_str());
  }

  void CheckValueInputIsTaggedOrPointer(Node const* node, int index) {
    Node const* input = node->InputAt(index);
    switch (inferrer_->GetRepresentation(input)) {
      case MachineRepresentation::kTagged:
      case MachineRepresentation::kTaggedPointer:
      case MachineRepresentation::kTaggedSigned:
        return;
      case MachineRepresentation::kBit:
      case MachineRepresentation::kWord8:
      case MachineRepresentation::kWord16:
      case MachineRepresentation::kWord32:
        if (Is32()) {
          return;
        }
        break;
      case MachineRepresentation::kWord64:
        if (Is64()) {
          return;
        }
        break;
      default:
        break;
    }
    if (inferrer_->GetRepresentation(input) !=
        MachineType::PointerRepresentation()) {
      std::ostringstream str;
      str << "TypeError: node #" << node->id() << ":" << *node->op()
          << " uses node #" << input->id() << ":" << *input->op()
          << " which doesn't have a tagged or pointer representation.";
      PrintDebugHelp(str, node);
      FATAL(str.str().c_str());
    }
  }

  void CheckValueInputForInt32Op(Node const* node, int index) {
    Node const* input = node->InputAt(index);
    switch (inferrer_->GetRepresentation(input)) {
      case MachineRepresentation::kBit:
      case MachineRepresentation::kWord8:
      case MachineRepresentation::kWord16:
      case MachineRepresentation::kWord32:
        return;
      case MachineRepresentation::kNone: {
        std::ostringstream str;
        str << "TypeError: node #" << input->id() << ":" << *input->op()
            << " is untyped.";
        PrintDebugHelp(str, node);
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
    PrintDebugHelp(str, node);
    FATAL(str.str().c_str());
  }

  void CheckValueInputForInt64Op(Node const* node, int index) {
    Node const* input = node->InputAt(index);
    MachineRepresentation input_representation =
        inferrer_->GetRepresentation(input);
    switch (input_representation) {
      case MachineRepresentation::kWord64:
        return;
      case MachineRepresentation::kNone: {
        std::ostringstream str;
        str << "TypeError: node #" << input->id() << ":" << *input->op()
            << " is untyped.";
        PrintDebugHelp(str, node);
        FATAL(str.str().c_str());
        break;
      }

      default:
        break;
    }
    std::ostringstream str;
    str << "TypeError: node #" << node->id() << ":" << *node->op()
        << " uses node #" << input->id() << ":" << *input->op() << ":"
        << input_representation
        << " which doesn't have a kWord64 representation.";
    PrintDebugHelp(str, node);
    FATAL(str.str().c_str());
  }

  void CheckValueInputForFloat32Op(Node const* node, int index) {
    Node const* input = node->InputAt(index);
    if (MachineRepresentation::kFloat32 ==
        inferrer_->GetRepresentation(input)) {
      return;
    }
    std::ostringstream str;
    str << "TypeError: node #" << node->id() << ":" << *node->op()
        << " uses node #" << input->id() << ":" << *input->op()
        << " which doesn't have a kFloat32 representation.";
    PrintDebugHelp(str, node);
    FATAL(str.str().c_str());
  }

  void CheckValueInputForFloat64Op(Node const* node, int index) {
    Node const* input = node->InputAt(index);
    if (MachineRepresentation::kFloat64 ==
        inferrer_->GetRepresentation(input)) {
      return;
    }
    std::ostringstream str;
    str << "TypeError: node #" << node->id() << ":" << *node->op()
        << " uses node #" << input->id() << ":" << *input->op()
        << " which doesn't have a kFloat64 representation.";
    PrintDebugHelp(str, node);
    FATAL(str.str().c_str());
  }

  void CheckCallInputs(Node const* node) {
    CallDescriptor const* desc = CallDescriptorOf(node->op());
    std::ostringstream str;
    bool should_log_error = false;
    for (size_t i = 0; i < desc->InputCount(); ++i) {
      Node const* input = node->InputAt(static_cast<int>(i));
      MachineRepresentation const input_type =
          inferrer_->GetRepresentation(input);
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
            << ") doesn't have a " << expected_input_type << " representation.";
      }
    }
    if (should_log_error) {
      PrintDebugHelp(str, node);
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

  void PrintDebugHelp(std::ostream& out, Node const* node) {
    if (DEBUG_BOOL) {
      out << "\n#     Current block: " << *current_block_;
      out << "\n#\n#     Specify option --csa-trap-on-node=" << name_ << ","
          << node->id() << " for debugging.";
    }
  }

  Schedule const* const schedule_;
  MachineRepresentationInferrer const* const inferrer_;
  bool is_stub_;
  const char* name_;
  BasicBlock* current_block_;
};

}  // namespace

void MachineGraphVerifier::Run(Graph* graph, Schedule const* const schedule,
                               Linkage* linkage, bool is_stub, const char* name,
                               Zone* temp_zone) {
  MachineRepresentationInferrer representation_inferrer(schedule, graph,
                                                        linkage, temp_zone);
  MachineRepresentationChecker checker(schedule, &representation_inferrer,
                                       is_stub, name);
  checker.Run();
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
