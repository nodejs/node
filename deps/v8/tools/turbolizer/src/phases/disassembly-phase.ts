// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { Phase, PhaseType } from "./phase";

export class DisassemblyPhase extends Phase {
  data: string;
  blockIdToOffset: Array<number>;
  blockStartPCtoBlockIds: Map<number, Array<number>>;

  constructor(name: string, data: string, blockIdToOffsetJson) {
    super(name, PhaseType.Disassembly);
    this.data = data;

    this.blockIdToOffset = new Array<number>();
    this.blockStartPCtoBlockIds = new Map<number, Array<number>>();
    this.parseBlockIdToOffsetFromJSON(blockIdToOffsetJson);
  }

  public hasBlockStartInfo(): boolean {
    return this.blockIdToOffset.length > 0;
  }

  public getBlockIdsForOffset(offset: number): Array<number> {
    return this.blockStartPCtoBlockIds.get(offset);
  }

  private parseBlockIdToOffsetFromJSON(blockIdToOffsetJson): void {
    if (!blockIdToOffsetJson) return;
    for (const [blockId, offset] of Object.entries<number>(blockIdToOffsetJson)) {
      this.blockIdToOffset[blockId] = offset;
      if (!this.blockStartPCtoBlockIds.has(offset)) {
        this.blockStartPCtoBlockIds.set(offset, new Array<number>());
      }
      this.blockStartPCtoBlockIds.get(offset).push(Number(blockId));
    }
  }
}
