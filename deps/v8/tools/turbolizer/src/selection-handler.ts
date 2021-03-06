// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface ClearableHandler {
  brokeredClear(): void;
}

export interface SelectionHandler {
  clear(): void;
  select(nodeIds: any, selected: any): void;
  brokeredSourcePositionSelect(sourcePositions: any, selected: any): void;
}

export interface NodeSelectionHandler {
  clear(): void;
  select(nodeIds: any, selected: any): void;
  brokeredNodeSelect(nodeIds: any, selected: any): void;
}

export interface BlockSelectionHandler {
  clear(): void;
  select(nodeIds: any, selected: any): void;
  brokeredBlockSelect(blockIds: any, selected: any): void;
}

export interface InstructionSelectionHandler {
  clear(): void;
  select(instructionIds: any, selected: any): void;
  brokeredInstructionSelect(instructionIds: any, selected: any): void;
}
