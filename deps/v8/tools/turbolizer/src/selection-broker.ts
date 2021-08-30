// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { SourceResolver, sourcePositionValid } from "../src/source-resolver";
import { ClearableHandler, SelectionHandler, NodeSelectionHandler, BlockSelectionHandler, InstructionSelectionHandler, RegisterAllocationSelectionHandler } from "../src/selection-handler";

export class SelectionBroker {
  sourceResolver: SourceResolver;
  allHandlers: Array<ClearableHandler>;
  sourcePositionHandlers: Array<SelectionHandler>;
  nodeHandlers: Array<NodeSelectionHandler>;
  blockHandlers: Array<BlockSelectionHandler>;
  instructionHandlers: Array<InstructionSelectionHandler>;
  registerAllocationHandlers: Array<RegisterAllocationSelectionHandler>;

  constructor(sourceResolver) {
    this.allHandlers = [];
    this.sourcePositionHandlers = [];
    this.nodeHandlers = [];
    this.blockHandlers = [];
    this.instructionHandlers = [];
    this.registerAllocationHandlers = [];
    this.sourceResolver = sourceResolver;
  }

  addSourcePositionHandler(handler: SelectionHandler & ClearableHandler) {
    this.allHandlers.push(handler);
    this.sourcePositionHandlers.push(handler);
  }

  addNodeHandler(handler: NodeSelectionHandler & ClearableHandler) {
    this.allHandlers.push(handler);
    this.nodeHandlers.push(handler);
  }

  addBlockHandler(handler: BlockSelectionHandler & ClearableHandler) {
    this.allHandlers.push(handler);
    this.blockHandlers.push(handler);
  }

  addInstructionHandler(handler: InstructionSelectionHandler & ClearableHandler) {
    this.allHandlers.push(handler);
    this.instructionHandlers.push(handler);
  }

  addRegisterAllocatorHandler(handler: RegisterAllocationSelectionHandler & ClearableHandler) {
    this.allHandlers.push(handler);
    this.registerAllocationHandlers.push(handler);
  }

  broadcastInstructionSelect(from, instructionOffsets, selected) {
    // Select the lines from the disassembly (right panel)
    for (const b of this.instructionHandlers) {
      if (b != from) b.brokeredInstructionSelect(instructionOffsets, selected);
    }

    // Select the lines from the source panel (left panel)
    const pcOffsets = this.sourceResolver.instructionsToKeyPcOffsets(instructionOffsets);
    for (const offset of pcOffsets) {
      const nodes = this.sourceResolver.nodesForPCOffset(offset)[0];
      const sourcePositions = this.sourceResolver.nodeIdsToSourcePositions(nodes);
      for (const b of this.sourcePositionHandlers) {
        if (b != from) b.brokeredSourcePositionSelect(sourcePositions, selected);
      }
    }

    // The middle panel lines have already been selected so there's no need to reselect them.
  }

  broadcastSourcePositionSelect(from, sourcePositions, selected) {
    sourcePositions = sourcePositions.filter(l => {
      if (!sourcePositionValid(l)) {
        console.log("Warning: invalid source position");
        return false;
      }
      return true;
    });

    // Select the lines from the source panel (left panel)
    for (const b of this.sourcePositionHandlers) {
      if (b != from) b.brokeredSourcePositionSelect(sourcePositions, selected);
    }

    // Select the nodes (middle panel)
    const nodes = this.sourceResolver.sourcePositionsToNodeIds(sourcePositions);
    for (const b of this.nodeHandlers) {
      if (b != from) b.brokeredNodeSelect(nodes, selected);
    }

    for (const node of nodes) {
      const instructionOffsets = this.sourceResolver.nodeIdToInstructionRange[node];
      // Skip nodes which do not have an associated instruction range.
      if (instructionOffsets == undefined) continue;

      // Select the lines from the disassembly (right panel)
      for (const b of this.instructionHandlers) {
        if (b != from) b.brokeredInstructionSelect(instructionOffsets, selected);
      }

      // Select the lines from the middle panel for the register allocation phase.
      for (const b of this.registerAllocationHandlers) {
        if (b != from) b.brokeredRegisterAllocationSelect(instructionOffsets, selected);
      }
    }
  }

  broadcastNodeSelect(from, nodes, selected) {
    // Select the nodes (middle panel)
    for (const b of this.nodeHandlers) {
      if (b != from) b.brokeredNodeSelect(nodes, selected);
    }

    // Select the lines from the source panel (left panel)
    const sourcePositions = this.sourceResolver.nodeIdsToSourcePositions(nodes);
    for (const b of this.sourcePositionHandlers) {
      if (b != from) b.brokeredSourcePositionSelect(sourcePositions, selected);
    }

    for (const node of nodes) {
      const instructionOffsets = this.sourceResolver.nodeIdToInstructionRange[node];
      // Skip nodes which do not have an associated instruction range.
      if (instructionOffsets == undefined) continue;
      // Select the lines from the disassembly (right panel)
      for (const b of this.instructionHandlers) {
        if (b != from) b.brokeredInstructionSelect(instructionOffsets, selected);
      }

      // Select the lines from the middle panel for the register allocation phase.
      for (const b of this.registerAllocationHandlers) {
        if (b != from) b.brokeredRegisterAllocationSelect(instructionOffsets, selected);
      }
    }
  }

  broadcastBlockSelect(from, blocks, selected) {
    for (const b of this.blockHandlers) {
      if (b != from) b.brokeredBlockSelect(blocks, selected);
    }
  }

  broadcastClear(from) {
    this.allHandlers.forEach(function (b) {
      if (b != from) b.brokeredClear();
    });
  }
}
