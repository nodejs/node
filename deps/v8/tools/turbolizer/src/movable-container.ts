// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { GraphPhase } from "./phases/graph-phase/graph-phase";
import { TurboshaftGraphPhase } from "./phases/turboshaft-graph-phase/turboshaft-graph-phase";

export abstract class MovableContainer<GraphPhaseType extends GraphPhase | TurboshaftGraphPhase> {
  graphPhase: GraphPhaseType;
  maxBackEdgeNumber: number;
  minGraphX: number;
  maxGraphX: number;
  maxGraphNodeX: number;
  minGraphY: number;
  maxGraphY: number;
  width: number;
  height: number;

  public abstract redetermineGraphBoundingBox(extendHeight: boolean):
    [[number, number], [number, number]];

  constructor(graphPhase: GraphPhaseType) {
    this.graphPhase = graphPhase;
    this.minGraphX = 0;
    this.maxGraphX = 1;
    this.minGraphY = 0;
    this.maxGraphY = 1;
    this.width = 1;
    this.height = 1;
  }

  public isRendered(): boolean {
    return this.graphPhase.rendered;
  }
}
