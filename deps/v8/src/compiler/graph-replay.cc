// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/graph-replay.h"

#include "src/compiler/all-nodes.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/graph.h"
#include "src/compiler/node.h"
#include "src/compiler/operator.h"
#include "src/compiler/operator-properties.h"

namespace v8 {
namespace internal {
namespace compiler {

#ifdef DEBUG

void GraphReplayPrinter::PrintReplay(Graph* graph) {
  GraphReplayPrinter replay;
  PrintF("  Node* nil = graph.NewNode(common_builder.Dead());\n");
  Zone zone;
  AllNodes nodes(&zone, graph);

  // Allocate the nodes first.
  for (Node* node : nodes.live) {
    PrintReplayOpCreator(node->op());
    PrintF("  Node* n%d = graph.NewNode(op", node->id());
    for (int i = 0; i < node->InputCount(); ++i) {
      PrintF(", nil");
    }
    PrintF("); USE(n%d);\n", node->id());
  }

  // Connect the nodes to their inputs.
  for (Node* node : nodes.live) {
    for (int i = 0; i < node->InputCount(); i++) {
      PrintF("  n%d->ReplaceInput(%d, n%d);\n", node->id(), i,
             node->InputAt(i)->id());
    }
  }
}


void GraphReplayPrinter::PrintReplayOpCreator(const Operator* op) {
  IrOpcode::Value opcode = static_cast<IrOpcode::Value>(op->opcode());
  const char* builder =
      IrOpcode::IsCommonOpcode(opcode) ? "common_builder" : "js_builder";
  const char* mnemonic = IrOpcode::IsCommonOpcode(opcode)
                             ? IrOpcode::Mnemonic(opcode)
                             : IrOpcode::Mnemonic(opcode) + 2;
  PrintF("  op = %s.%s(", builder, mnemonic);
  switch (opcode) {
    case IrOpcode::kParameter:
    case IrOpcode::kNumberConstant:
      PrintF("0");
      break;
    case IrOpcode::kLoad:
      PrintF("unique_name");
      break;
    case IrOpcode::kHeapConstant:
      PrintF("unique_constant");
      break;
    case IrOpcode::kPhi:
      PrintF("%d", op->ValueInputCount());
      break;
    case IrOpcode::kEffectPhi:
      PrintF("%d", op->EffectInputCount());
      break;
    case IrOpcode::kLoop:
    case IrOpcode::kMerge:
      PrintF("%d", op->ControlInputCount());
      break;
    default:
      break;
  }
  PrintF(");\n");
}

#endif  // DEBUG
}
}
}  // namespace v8::internal::compiler
