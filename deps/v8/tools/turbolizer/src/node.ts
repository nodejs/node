// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as C from "./common/constants";
import { measureText } from "./common/util";
import { GraphEdge } from "./phases/graph-phase/graph-edge";
import { TurboshaftGraphEdge } from "./phases/turboshaft-graph-phase/turboshaft-graph-edge";
import { TurboshaftGraphNode } from "./phases/turboshaft-graph-phase/turboshaft-graph-node";
import { TurboshaftGraphBlock } from "./phases/turboshaft-graph-phase/turboshaft-graph-block";

export abstract class Node<EdgeType extends GraphEdge | TurboshaftGraphEdge<TurboshaftGraphNode
  | TurboshaftGraphBlock>> {
  id: number;
  displayLabel: string;
  inputs: Array<EdgeType>;
  outputs: Array<EdgeType>;
  visible: boolean;
  x: number;
  y: number;
  labelBox: { width: number, height: number };

  public abstract getHeight(extendHeight: boolean): number;
  public abstract getWidth(): number;

  constructor(id: number, displayLabel?: string) {
    this.id = id;
    this.displayLabel = displayLabel;
    this.inputs = new Array<EdgeType>();
    this.outputs = new Array<EdgeType>();
    this.visible = false;
    this.x = 0;
    this.y = 0;
    this.labelBox = measureText(this.displayLabel);
  }

  public areAnyOutputsVisible(): OutputVisibilityType {
    let visibleCount = 0;
    for (const edge of this.outputs) {
      if (edge.isVisible()) {
        ++visibleCount;
      }
    }
    if (this.outputs.length == visibleCount) {
      return OutputVisibilityType.AllNodesVisible;
    }
    if (visibleCount != 0) {
      return OutputVisibilityType.SomeNodesVisible;
    }
    return OutputVisibilityType.NoVisibleNodes;
  }

  public setOutputVisibility(visibility: boolean): boolean {
    let result = false;
    for (const edge of this.outputs) {
      edge.visible = visibility;
      if (visibility && !edge.target.visible) {
        edge.target.visible = true;
        result = true;
      }
    }
    return result;
  }

  public setInputVisibility(edgeIdx: number, visibility: boolean): boolean {
    const edge = this.inputs[edgeIdx];
    edge.visible = visibility;
    if (visibility && !edge.source.visible) {
      edge.source.visible = true;
      return true;
    }
    return false;
  }

  public getInputX(index: number): number {
    return this.getWidth() - (C.NODE_INPUT_WIDTH / 2) +
      (index - this.inputs.length + 1) * C.NODE_INPUT_WIDTH;
  }

  public getOutputX(): number {
    return this.getWidth() - (C.NODE_INPUT_WIDTH / 2);
  }

  public identifier(): string {
    return `${this.id}`;
  }

  public toString(): string {
    return `N${this.id}`;
  }
}

export enum OutputVisibilityType {
  NoVisibleNodes,
  SomeNodesVisible,
  AllNodesVisible
}
