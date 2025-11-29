// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export abstract class Phase {
  name: string;
  type: PhaseType;

  constructor(name, type: PhaseType) {
    this.name = name;
    this.type = type;
  }

  public isGraph(): boolean {
    return this.type == PhaseType.Graph || this.type == PhaseType.TurboshaftGraph;
  }

  public isDynamic(): boolean {
    return this.isGraph() || this.type == PhaseType.Schedule || this.type == PhaseType.Sequence;
  }
}

export enum PhaseType {
  Graph = "graph",
  TurboshaftGraph = "turboshaft_graph",
  TurboshaftCustomData = "turboshaft_custom_data",
  Disassembly = "disassembly",
  Instructions = "instructions",
  Sequence = "sequence",
  Schedule = "schedule"
}

export enum GraphStateType {
  NeedToFullRebuild,
  Cached
}
