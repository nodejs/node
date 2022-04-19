// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { GNode, MINIMUM_EDGE_SEPARATION, DEFAULT_NODE_BUBBLE_RADIUS } from "../src/node";
import { Graph } from "./graph";

const BEZIER_CONSTANT = 0.3;

export class Edge {
  target: GNode;
  source: GNode;
  index: number;
  type: string;
  backEdgeNumber: number;
  visible: boolean;

  constructor(target: GNode, index: number, source: GNode, type: string) {
    this.target = target;
    this.source = source;
    this.index = index;
    this.type = type;
    this.backEdgeNumber = 0;
    this.visible = false;
  }

  stringID() {
    return this.source.id + "," + this.index + "," + this.target.id;
  }

  isVisible() {
    return this.visible && this.source.visible && this.target.visible;
  }

  getInputHorizontalPosition(graph: Graph, showTypes: boolean) {
    if (this.backEdgeNumber > 0) {
      return graph.maxGraphNodeX + this.backEdgeNumber * MINIMUM_EDGE_SEPARATION;
    }
    const source = this.source;
    const target = this.target;
    const index = this.index;
    const inputX = target.x + target.getInputX(index);
    const inputApproach = target.getInputApproach(this.index);
    const outputApproach = source.getOutputApproach(showTypes);
    if (inputApproach > outputApproach) {
      return inputX;
    } else {
      const inputOffset = MINIMUM_EDGE_SEPARATION * (index + 1);
      return (target.x < source.x)
        ? (target.x + target.getTotalNodeWidth() + inputOffset)
        : (target.x - inputOffset);
    }
  }

  generatePath(graph: Graph, showTypes: boolean) {
    const target = this.target;
    const source = this.source;
    const inputX = target.x + target.getInputX(this.index);
    const arrowheadHeight = 7;
    const inputY = target.y - 2 * DEFAULT_NODE_BUBBLE_RADIUS - arrowheadHeight;
    const outputX = source.x + source.getOutputX();
    const outputY = source.y + source.getNodeHeight(showTypes) + DEFAULT_NODE_BUBBLE_RADIUS;
    let inputApproach = target.getInputApproach(this.index);
    const outputApproach = source.getOutputApproach(showTypes);
    const horizontalPos = this.getInputHorizontalPosition(graph, showTypes);

    let result: string;

    if (inputY < outputY) {
      result = `M ${outputX} ${outputY}
                L ${outputX} ${outputApproach}
                L ${horizontalPos} ${outputApproach}`;

      if (horizontalPos != inputX) {
        result += `L ${horizontalPos} ${inputApproach}`;
      } else {
        if (inputApproach < outputApproach) {
          inputApproach = outputApproach;
        }
      }

      result += `L ${inputX} ${inputApproach}
                 L ${inputX} ${inputY}`;
    } else {
      const controlY = outputY + (inputY - outputY) * BEZIER_CONSTANT;
      result = `M ${outputX} ${outputY}
                C ${outputX} ${controlY},
                  ${inputX} ${outputY},
                  ${inputX} ${inputY}`;
    }

    return result;
  }

  isBackEdge() {
    return this.target.hasBackEdges() && (this.target.rank < this.source.rank);
  }

}

export const edgeToStr = (e: Edge) => e.stringID();
