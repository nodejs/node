// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { MINIMUM_EDGE_SEPARATION, Edge } from "../src/edge";
import { NodeLabel } from "./node-label";
import { MAX_RANK_SENTINEL } from "./constants";
import { alignUp, measureText } from "./util";

export const DEFAULT_NODE_BUBBLE_RADIUS = 12;
export const NODE_INPUT_WIDTH = 50;
export const MINIMUM_NODE_OUTPUT_APPROACH = 15;
const MINIMUM_NODE_INPUT_APPROACH = 15 + 2 * DEFAULT_NODE_BUBBLE_RADIUS;

export class GNode {
  id: number;
  nodeLabel: NodeLabel;
  displayLabel: string;
  inputs: Array<Edge>;
  outputs: Array<Edge>;
  visible: boolean;
  x: number;
  y: number;
  rank: number;
  outputApproach: number;
  cfg: boolean;
  labelbbox: { width: number, height: number };
  width: number;
  normalheight: number;
  visitOrderWithinRank: number;

  constructor(nodeLabel: NodeLabel) {
    this.id = nodeLabel.id;
    this.nodeLabel = nodeLabel;
    this.displayLabel = nodeLabel.getDisplayLabel();
    this.inputs = [];
    this.outputs = [];
    this.visible = false;
    this.x = 0;
    this.y = 0;
    this.rank = MAX_RANK_SENTINEL;
    this.outputApproach = MINIMUM_NODE_OUTPUT_APPROACH;
    // Every control node is a CFG node.
    this.cfg = nodeLabel.control;
    this.labelbbox = measureText(this.displayLabel);
    const typebbox = measureText(this.getDisplayType());
    const innerwidth = Math.max(this.labelbbox.width, typebbox.width);
    this.width = alignUp(innerwidth + NODE_INPUT_WIDTH * 2,
      NODE_INPUT_WIDTH);
    const innerheight = Math.max(this.labelbbox.height, typebbox.height);
    this.normalheight = innerheight + 20;
    this.visitOrderWithinRank = 0;
  }

  isControl() {
    return this.nodeLabel.control;
  }
  isInput() {
    return this.nodeLabel.opcode == 'Parameter' || this.nodeLabel.opcode.endsWith('Constant');
  }
  isLive() {
    return this.nodeLabel.live !== false;
  }
  isJavaScript() {
    return this.nodeLabel.opcode.startsWith('JS');
  }
  isSimplified() {
    if (this.isJavaScript()) return false;
    const opcode = this.nodeLabel.opcode;
    return opcode.endsWith('Phi') ||
      opcode.startsWith('Boolean') ||
      opcode.startsWith('Number') ||
      opcode.startsWith('String') ||
      opcode.startsWith('Change') ||
      opcode.startsWith('Object') ||
      opcode.startsWith('Reference') ||
      opcode.startsWith('Any') ||
      opcode.endsWith('ToNumber') ||
      (opcode == 'AnyToBoolean') ||
      (opcode.startsWith('Load') && opcode.length > 4) ||
      (opcode.startsWith('Store') && opcode.length > 5);
  }
  isMachine() {
    return !(this.isControl() || this.isInput() ||
      this.isJavaScript() || this.isSimplified());
  }
  getTotalNodeWidth() {
    const inputWidth = this.inputs.length * NODE_INPUT_WIDTH;
    return Math.max(inputWidth, this.width);
  }
  getTitle() {
    return this.nodeLabel.getTitle();
  }
  getDisplayLabel() {
    return this.nodeLabel.getDisplayLabel();
  }
  getType() {
    return this.nodeLabel.type;
  }
  getDisplayType() {
    let typeString = this.nodeLabel.type;
    if (typeString == undefined) return "";
    if (typeString.length > 24) {
      typeString = typeString.substr(0, 25) + "...";
    }
    return typeString;
  }
  deepestInputRank() {
    let deepestRank = 0;
    this.inputs.forEach(function (e) {
      if (e.isVisible() && !e.isBackEdge()) {
        if (e.source.rank > deepestRank) {
          deepestRank = e.source.rank;
        }
      }
    });
    return deepestRank;
  }
  areAnyOutputsVisible() {
    let visibleCount = 0;
    this.outputs.forEach(function (e) { if (e.isVisible())++visibleCount; });
    if (this.outputs.length == visibleCount) return 2;
    if (visibleCount != 0) return 1;
    return 0;
  }
  setOutputVisibility(v) {
    let result = false;
    this.outputs.forEach(function (e) {
      e.visible = v;
      if (v) {
        if (!e.target.visible) {
          e.target.visible = true;
          result = true;
        }
      }
    });
    return result;
  }
  setInputVisibility(i, v) {
    const edge = this.inputs[i];
    edge.visible = v;
    if (v) {
      if (!edge.source.visible) {
        edge.source.visible = true;
        return true;
      }
    }
    return false;
  }
  getInputApproach(index) {
    return this.y - MINIMUM_NODE_INPUT_APPROACH -
      (index % 4) * MINIMUM_EDGE_SEPARATION - DEFAULT_NODE_BUBBLE_RADIUS;
  }
  getNodeHeight(showTypes: boolean): number {
    if (showTypes) {
      return this.normalheight + this.labelbbox.height;
    } else {
      return this.normalheight;
    }
  }
  getOutputApproach(showTypes: boolean) {
    return this.y + this.outputApproach + this.getNodeHeight(showTypes) +
      + DEFAULT_NODE_BUBBLE_RADIUS;
  }
  getInputX(index) {
    const result = this.getTotalNodeWidth() - (NODE_INPUT_WIDTH / 2) +
      (index - this.inputs.length + 1) * NODE_INPUT_WIDTH;
    return result;
  }
  getOutputX() {
    return this.getTotalNodeWidth() - (NODE_INPUT_WIDTH / 2);
  }
  hasBackEdges() {
    return (this.nodeLabel.opcode == "Loop") ||
      ((this.nodeLabel.opcode == "Phi" || this.nodeLabel.opcode == "EffectPhi" || this.nodeLabel.opcode == "InductionVariablePhi") &&
        this.inputs[this.inputs.length - 1].source.nodeLabel.opcode == "Loop");
  }
}

export const nodeToStr = (n: GNode) => "N" + n.id;
