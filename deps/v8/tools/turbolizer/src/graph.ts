// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as C from "./common/constants";
import { GraphPhase, GraphStateType } from "./phases/graph-phase/graph-phase";
import { GraphEdge } from "./phases/graph-phase/graph-edge";
import { GraphNode } from "./phases/graph-phase/graph-node";
import { MovableContainer } from "./movable-container";

export class Graph extends MovableContainer<GraphPhase> {
  nodeMap: Array<GraphNode>;
  originNodesMap: Map<string, Array<GraphNode>>;

  constructor(graphPhase: GraphPhase) {
    super(graphPhase);
    this.nodeMap = graphPhase.nodeIdToNodeMap;
    this.originNodesMap = graphPhase.originIdToNodesMap;
  }

  public *nodes(func = (n: GraphNode) => true) {
    for (const node of this.nodeMap) {
      if (!node || !func(node)) continue;
      yield node;
    }
  }

  public *filteredEdges(func: (e: GraphEdge) => boolean) {
    for (const node of this.nodes()) {
      for (const edge of node.inputs) {
        if (func(edge)) yield edge;
      }
    }
  }

  public forEachEdge(func: (e: GraphEdge) => void) {
    for (const node of this.nodeMap) {
      if (!node) continue;
      for (const edge of node.inputs) {
        func(edge);
      }
    }
  }

  public redetermineGraphBoundingBox(showTypes: boolean): [[number, number], [number, number]] {
    this.minGraphX = 0;
    this.maxGraphNodeX = 1;
    this.minGraphY = 0;
    this.maxGraphY = 1;

    for (const node of this.nodes()) {
      if (!node.visible) continue;

      this.minGraphX = Math.min(this.minGraphX, node.x);
      this.maxGraphNodeX = Math.max(this.maxGraphNodeX, node.x + node.getWidth());

      this.minGraphY = Math.min(this.minGraphY, node.y - C.NODE_INPUT_WIDTH);
      this.maxGraphY = Math.max(this.maxGraphY, node.y + node.getHeight(showTypes)
        + C.NODE_INPUT_WIDTH);
    }

    this.maxGraphX = this.maxGraphNodeX + this.maxBackEdgeNumber
      * C.MINIMUM_EDGE_SEPARATION;

    this.width = this.maxGraphX - this.minGraphX;
    this.height = this.maxGraphY - this.minGraphY;

    return [
      [this.minGraphX - this.width / 2, this.minGraphY - this.height / 2],
      [this.maxGraphX + this.width / 2, this.maxGraphY + this.height / 2]
    ];
  }

  public makeNodeVisible(identifier: string): void {
    if (this.nodeMap[identifier]) this.nodeMap[identifier].visible = true;
  }

  public makeEdgesVisible(): void {
    if (this.graphPhase.stateType == GraphStateType.NeedToFullRebuild) {
      this.forEachEdge(edge =>
        edge.visible = edge.source.visible && edge.target.visible
      );
    } else {
      this.forEachEdge(edge =>
        edge.visible = edge.visible || (this.isRendered() &&
          edge.type === "control" && edge.source.visible && edge.target.visible)
      );
    }
  }
}
