// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export abstract class Origin {
  phase: string;
  reducer: string;

  constructor(phase: string, reducer: string) {
    this.phase = phase;
    this.reducer = reducer;
  }
}

export class NodeOrigin extends Origin {
  nodeId: number;

  constructor(nodeId: number, phase: string, reducer: string) {
    super(phase, reducer);
    this.nodeId = nodeId;
  }

  public identifier(): string {
    return `${this.nodeId}`;
  }

  public toString(): string {
    return `#${this.nodeId} in phase '${this.phase}/${this.reducer}'`;
  }
}

export class BytecodeOrigin extends Origin {
  bytecodePosition: number;

  constructor(bytecodePosition: number, phase: string, reducer: string) {
    super(phase, reducer);
    this.bytecodePosition = bytecodePosition;
  }

  public identifier(): string {
    return `${this.bytecodePosition}`;
  }

  public toString(): string {
    return `Bytecode line ${this.bytecodePosition} in phase '${this.phase}/${this.reducer}'`;
  }
}
