// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as C from "./common/constants";
import { MovableContainer } from "./movable-container";
import { TurboshaftGraphPhase } from "./phases/turboshaft-graph-phase/turboshaft-graph-phase";
import { TurboshaftGraphNode } from "./phases/turboshaft-graph-phase/turboshaft-graph-node";
import { TurboshaftGraphBlock } from "./phases/turboshaft-graph-phase/turboshaft-graph-block";
import { TurboshaftGraphEdge } from "./phases/turboshaft-graph-phase/turboshaft-graph-edge";

export class TurboshaftGraph extends MovableContainer<TurboshaftGraphPhase> {
  blockMap: Array<TurboshaftGraphBlock>;
  nodeMap: Array<TurboshaftGraphNode>;

  constructor(graphPhase: TurboshaftGraphPhase) {
    super(graphPhase);
    this.blockMap = graphPhase.blockIdToBlockMap;
    this.nodeMap = graphPhase.nodeIdToNodeMap;
  }

  public *blocks(func = (b: TurboshaftGraphBlock) => true) {
    for (const block of this.blockMap) {
      if (!block || !func(block)) continue;
      yield block;
    }
  }

  public *nodes(func = (n: TurboshaftGraphNode) => true) {
    for (const node of this.nodeMap) {
      if (!node || !func(node)) continue;
      yield node;
    }
  }

  public *blocksEdges(func = (e: TurboshaftGraphEdge<TurboshaftGraphBlock>) => true) {
    for (const block of this.blockMap) {
      if (!block) continue;
      for (const edge of block.inputs) {
        if (!edge || func(edge)) continue;
        yield edge;
      }
    }
  }

  public redetermineGraphBoundingBox(showProperties: boolean):
    [[number, number], [number, number]] {
    this.minGraphX = 0;
    this.maxGraphNodeX = 1;
    this.minGraphY = 0;
    this.maxGraphY = 1;

    for (const block of this.blocks()) {
      if (!block.visible) continue;

      this.minGraphX = Math.min(this.minGraphX, block.x);
      this.maxGraphNodeX = Math.max(this.maxGraphNodeX, block.x + block.getWidth());

      this.minGraphY = Math.min(this.minGraphY, block.y - C.NODE_INPUT_WIDTH);
      this.maxGraphY = Math.max(this.maxGraphY, block.y + block.getHeight(showProperties)
        + C.NODE_INPUT_WIDTH);
    }

    this.maxGraphX = this.maxGraphNodeX + 3 * C.MINIMUM_EDGE_SEPARATION;

    this.width = this.maxGraphX - this.minGraphX;
    this.height = this.maxGraphY - this.minGraphY;

    return [
      [this.minGraphX - this.width / 2, this.minGraphY - this.height / 2],
      [this.maxGraphX + this.width / 2, this.maxGraphY + this.height / 2]
    ];
  }
}
