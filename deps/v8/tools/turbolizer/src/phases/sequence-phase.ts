// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { Phase, PhaseType } from "./phase";

export class SequencePhase extends Phase {
  blocks: Array<any>;
  registerAllocation: RegisterAllocation;

  constructor(name: string, blocks: Array<any>, registerAllocation?: RegisterAllocation) {
    super(name, PhaseType.Sequence);
    this.blocks = blocks;
    this.registerAllocation = registerAllocation;
  }
}

export interface RegisterAllocation {
  fixedDoubleLiveRanges: Map<string, Range>;
  fixedLiveRanges: Map<string, Range>;
  liveRanges: Map<string, Range>;
}

export interface Range {
  childRanges: Array<ChildRange>;
  isDeferred: boolean;
}

export interface ChildRange {
  id: string;
  type: string;
  op: any;
  intervals: Array<[number, number]>;
  uses: Array<number>;
}
