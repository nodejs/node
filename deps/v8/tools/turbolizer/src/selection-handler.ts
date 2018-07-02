// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

interface SelectionHandler {
  clear(): void;
  select(nodeIds: any, selected: any): void;
  brokeredClear(): void;
  brokeredSourcePositionSelect(sourcePositions: any, selected: any): void;
};

interface NodeSelectionHandler {
  clear(): void;
  select(nodeIds: any, selected: any): void;
  brokeredClear(): void;
  brokeredNodeSelect(nodeIds: any, selected: any): void;
};

interface BlockSelectionHandler {
  clear(): void;
  select(nodeIds: any, selected: any): void;
  brokeredClear(): void;
  brokeredBlockSelect(blockIds: any, selected: any): void;
};
