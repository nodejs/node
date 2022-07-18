// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as C from "../../common/constants";
import { Graph } from "../../graph";
import { Edge } from "../../edge";
import { GraphNode } from "./graph-node";

export class GraphEdge extends Edge<GraphNode> {
  index: number;
  type: string;

  constructor(target: GraphNode, index: number, source: GraphNode, type: string) {
    super(target, source);
    this.index = index;
    this.type = type;
  }

  public getInputHorizontalPosition(graph: Graph, showTypes: boolean): number {
    if (graph.graphPhase.rendered && this.backEdgeNumber > 0) {
      return graph.maxGraphNodeX + this.backEdgeNumber * C.MINIMUM_EDGE_SEPARATION;
    }
    const source = this.source;
    const target = this.target;
    const index = this.index;
    const inputX = target.x + target.getInputX(index);
    const inputApproach = target.getInputApproach(this.index);
    const outputApproach = source.getOutputApproach(showTypes);
    if (inputApproach > outputApproach) {
      return inputX;
    }
    const inputOffset = C.MINIMUM_EDGE_SEPARATION * (index + 1);
    return target.x < source.x
      ? target.x + target.getWidth() + inputOffset
      : target.x - inputOffset;
  }

  public generatePath(graph: Graph, showTypes: boolean): string {
    const target = this.target;
    const source = this.source;
    const inputX = target.x + target.getInputX(this.index);
    const arrowheadHeight = 7;
    const inputY = target.y - 2 * C.DEFAULT_NODE_BUBBLE_RADIUS - arrowheadHeight;
    const outputX = source.x + source.getOutputX();
    const outputY = source.y + source.getHeight(showTypes) + C.DEFAULT_NODE_BUBBLE_RADIUS;
    let inputApproach = target.getInputApproach(this.index);
    const outputApproach = source.getOutputApproach(showTypes);
    const horizontalPos = this.getInputHorizontalPosition(graph, showTypes);

    let path: string;

    if (inputY < outputY) {
      path = `M ${outputX} ${outputY}\nL ${outputX} ${outputApproach}\nL ${horizontalPos} ${outputApproach}`;
      if (horizontalPos !== inputX) {
        path += `L ${horizontalPos} ${inputApproach}`;
      } else if (inputApproach < outputApproach) {
        inputApproach = outputApproach;
      }
      path += `L ${inputX} ${inputApproach}\nL ${inputX} ${inputY}`;
    } else {
      const controlY = outputY + (inputY - outputY) * C.BEZIER_CONSTANT;
      path = `M ${outputX} ${outputY}\nC ${outputX} ${controlY},\n${inputX} ${outputY},\n${inputX} ${inputY}`;
    }

    return path;
  }

  public isBackEdge(): boolean {
    return this.target.hasBackEdges() && (this.target.rank < this.source.rank);
  }

  public toString(): string {
    return `${this.source.id},${this.index},${this.target.id}`;
  }
}
