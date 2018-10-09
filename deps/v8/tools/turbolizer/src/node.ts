// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NodeOrigin} from "./source-resolver.js"
import {MINIMUM_EDGE_SEPARATION} from "./edge.js"

export const DEFAULT_NODE_BUBBLE_RADIUS = 12;
export const NODE_INPUT_WIDTH = 50;
export const MINIMUM_NODE_OUTPUT_APPROACH = 15;
const MINIMUM_NODE_INPUT_APPROACH = 15 + 2 * DEFAULT_NODE_BUBBLE_RADIUS;

export function isNodeInitiallyVisible(node) {
  return node.cfg;
}

function formatOrigin(origin) {
  if (origin.nodeId) {
    return `#${origin.nodeId} in phase ${origin.phase}/${origin.reducer}`;
  }
  if (origin.bytecodePosition) {
    return `Bytecode line ${origin.bytecodePosition} in phase ${origin.phase}/${origin.reducer}`;
  }
  return "unknown origin";
}

export class GNode {
  control: boolean;
  opcode: string;
  live: boolean;
  inputs: Array<any>;
  width: number;
  properties: string;
  title: string;
  label: string;
  origin: NodeOrigin;
  outputs: Array<any>;
  outputApproach: number;
  type: string;
  id: number;
  x: number;
  y: number;
  visible: boolean;
  rank: number;
  opinfo: string;
  labelbbox: { width: number, height: number };

  isControl() {
    return this.control;
  }
  isInput() {
    return this.opcode == 'Parameter' || this.opcode.endsWith('Constant');
  }
  isLive() {
    return this.live !== false;
  }
  isJavaScript() {
    return this.opcode.startsWith('JS');
  }
  isSimplified() {
    if (this.isJavaScript()) return false;
    return this.opcode.endsWith('Phi') ||
      this.opcode.startsWith('Boolean') ||
      this.opcode.startsWith('Number') ||
      this.opcode.startsWith('String') ||
      this.opcode.startsWith('Change') ||
      this.opcode.startsWith('Object') ||
      this.opcode.startsWith('Reference') ||
      this.opcode.startsWith('Any') ||
      this.opcode.endsWith('ToNumber') ||
      (this.opcode == 'AnyToBoolean') ||
      (this.opcode.startsWith('Load') && this.opcode.length > 4) ||
      (this.opcode.startsWith('Store') && this.opcode.length > 5);
  }
  isMachine() {
    return !(this.isControl() || this.isInput() ||
      this.isJavaScript() || this.isSimplified());
  }
  getTotalNodeWidth() {
    var inputWidth = this.inputs.length * NODE_INPUT_WIDTH;
    return Math.max(inputWidth, this.width);
  }
  getTitle() {
    var propsString;
    if (this.properties === undefined) {
      propsString = "";
    } else if (this.properties === "") {
      propsString = "no properties";
    } else {
      propsString = "[" + this.properties + "]";
    }
    let title = this.title + "\n" + propsString + "\n" + this.opinfo;
    if (this.origin) {
      title += `\nOrigin: ${formatOrigin(this.origin)}`;
    }
    return title;
  }
  getDisplayLabel() {
    var result = this.id + ":" + this.label;
    if (result.length > 40) {
      return this.id + ":" + this.opcode;
    } else {
      return result;
    }
  }
  getType() {
    return this.type;
  }
  getDisplayType() {
    var type_string = this.type;
    if (type_string == undefined) return "";
    if (type_string.length > 24) {
      type_string = type_string.substr(0, 25) + "...";
    }
    return type_string;
  }
  deepestInputRank() {
    var deepestRank = 0;
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
    var visibleCount = 0;
    this.outputs.forEach(function (e) { if (e.isVisible())++visibleCount; });
    if (this.outputs.length == visibleCount) return 2;
    if (visibleCount != 0) return 1;
    return 0;
  }
  setOutputVisibility(v) {
    var result = false;
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
    var edge = this.inputs[i];
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
      (index % 4) * MINIMUM_EDGE_SEPARATION - DEFAULT_NODE_BUBBLE_RADIUS
  }
  getOutputApproach(graph) {
    return this.y + this.outputApproach + graph.getNodeHeight(this) +
      + DEFAULT_NODE_BUBBLE_RADIUS;
  }
  getInputX(index) {
    var result = this.getTotalNodeWidth() - (NODE_INPUT_WIDTH / 2) +
      (index - this.inputs.length + 1) * NODE_INPUT_WIDTH;
    return result;
  }
  getOutputX() {
    return this.getTotalNodeWidth() - (NODE_INPUT_WIDTH / 2);
  }
  hasBackEdges() {
    return (this.opcode == "Loop") ||
      ((this.opcode == "Phi" || this.opcode == "EffectPhi") &&
        this.inputs[this.inputs.length - 1].source.opcode == "Loop");
  }
};

export const nodeToStr = (n: GNode) => "N" + n.id;
