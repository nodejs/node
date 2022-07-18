// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as C from "../../common/constants";
import { alignUp, measureText } from "../../common/util";
import { NodeLabel } from "../../node-label";
import { Node } from "../../node";
import { GraphEdge } from "./graph-edge";

export class GraphNode extends Node<GraphEdge> {
  nodeLabel: NodeLabel;
  rank: number;
  outputApproach: number;
  cfg: boolean;
  width: number;
  normalHeight: number;
  visitOrderWithinRank: number;

  constructor(nodeLabel: NodeLabel) {
    super(nodeLabel.id, nodeLabel.getDisplayLabel());
    this.nodeLabel = nodeLabel;
    this.rank = C.MAX_RANK_SENTINEL;
    this.outputApproach = C.MINIMUM_NODE_OUTPUT_APPROACH;
    // Every control node is a CFG node.
    this.cfg = nodeLabel.control;
    const typeBox = measureText(this.getDisplayType());
    const innerWidth = Math.max(this.labelBox.width, typeBox.width);
    this.width = alignUp(innerWidth + C.NODE_INPUT_WIDTH * 2, C.NODE_INPUT_WIDTH);
    const innerHeight = Math.max(this.labelBox.height, typeBox.height);
    this.normalHeight = innerHeight + 20;
    this.visitOrderWithinRank = 0;
  }

  public getHeight(showTypes: boolean): number {
    if (showTypes) {
      return this.normalHeight + this.labelBox.height;
    }
    return this.normalHeight;
  }

  public getWidth(): number {
    return Math.max(this.inputs.length * C.NODE_INPUT_WIDTH, this.width);
  }

  public isControl(): boolean {
    return this.nodeLabel.control;
  }

  public isInput(): boolean {
    return this.nodeLabel.opcode === "Parameter" || this.nodeLabel.opcode.endsWith("Constant");
  }

  public isLive(): boolean {
    return this.nodeLabel.live !== false;
  }

  public isJavaScript(): boolean {
    return this.nodeLabel.opcode.startsWith("JS");
  }

  public isSimplified(): boolean {
    if (this.isJavaScript()) return false;
    const opcode = this.nodeLabel.opcode;
    return opcode.endsWith("Phi") ||
      opcode.startsWith("Boolean") ||
      opcode.startsWith("Number") ||
      opcode.startsWith("String") ||
      opcode.startsWith("Change") ||
      opcode.startsWith("Object") ||
      opcode.startsWith("Reference") ||
      opcode.startsWith("Any") ||
      opcode.endsWith("ToNumber") ||
      (opcode === "AnyToBoolean") ||
      (opcode.startsWith("Load") && opcode.length > 4) ||
      (opcode.startsWith("Store") && opcode.length > 5);
  }

  public isMachine(): boolean {
    return !(this.isControl() || this.isInput() ||
      this.isJavaScript() || this.isSimplified());
  }

  public getTitle(): string {
    return this.nodeLabel.getTitle();
  }

  public getDisplayLabel(): string {
    return this.nodeLabel.getDisplayLabel();
  }

  public getType(): string {
    return this.nodeLabel.type;
  }

  public getDisplayType(): string {
    const typeString = this.nodeLabel.type;
    if (typeString == undefined) return "";
    return typeString.length > 24 ? `${typeString.slice(0, 25)}...` : typeString;
  }

  public deepestInputRank(): number {
    let deepestRank = 0;
    for (const edge of this.inputs) {
      if ((edge.isVisible() && !edge.isBackEdge()) && edge.source.rank > deepestRank) {
        deepestRank = edge.source.rank;
      }
    }
    return deepestRank;
  }

  public getInputApproach(index: number): number {
    return this.y - C.MINIMUM_NODE_INPUT_APPROACH -
      (index % 4) * C.MINIMUM_EDGE_SEPARATION - C.DEFAULT_NODE_BUBBLE_RADIUS;
  }

  public getOutputApproach(showTypes: boolean): number {
    return this.y + this.outputApproach + this.getHeight(showTypes) +
      + C.DEFAULT_NODE_BUBBLE_RADIUS;
  }

  public hasBackEdges(): boolean {
    return (this.nodeLabel.opcode === "Loop") ||
      ((this.nodeLabel.opcode === "Phi" || this.nodeLabel.opcode === "EffectPhi" ||
          this.nodeLabel.opcode === "InductionVariablePhi") &&
        this.inputs[this.inputs.length - 1].source.nodeLabel.opcode === "Loop");
  }

  public compare(other: GraphNode): number {
    if (this.visitOrderWithinRank < other.visitOrderWithinRank) {
      return -1;
    } else if (this.visitOrderWithinRank == other.visitOrderWithinRank) {
      return 0;
    }
    return 1;
  }
}
