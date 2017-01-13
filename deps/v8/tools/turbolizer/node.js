// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var TYPE_HEIGHT = 25;
var DEFAULT_NODE_BUBBLE_RADIUS = 12;
var NODE_INPUT_WIDTH = 50;
var MINIMUM_NODE_INPUT_APPROACH = 15 + 2 * DEFAULT_NODE_BUBBLE_RADIUS;
var MINIMUM_NODE_OUTPUT_APPROACH = 15;

function isNodeInitiallyVisible(node) {
  return node.cfg;
}

var Node = {
  isControl: function() {
    return this.control;
  },
  isInput: function() {
    return this.opcode == 'Parameter' || this.opcode.endsWith('Constant');
  },
  isLive: function() {
    return this.live !== false;
  },
  isJavaScript: function() {
    return this.opcode.startsWith('JS');
  },
  isSimplified: function() {
    if (this.isJavaScript) return false;
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
  },
  isMachine: function() {
    return !(this.isControl() || this.isInput() ||
             this.isJavaScript() || this.isSimplified());
  },
  getTotalNodeWidth: function() {
    var inputWidth = this.inputs.length * NODE_INPUT_WIDTH;
    return Math.max(inputWidth, this.width);
  },
  getTitle: function() {
    var propsString;
    if (this.properties === undefined) {
      propsString = "";
    } else if (this.properties === "") {
      propsString = "no properties";
    } else {
      propsString = "[" + this.properties + "]";
    }
    return this.title + "\n" + propsString + "\n" + this.opinfo;
  },
  getDisplayLabel: function() {
    var result = this.id + ":" + this.label;
    if (result.length > 40) {
      return this.id + ":" + this.opcode;
    } else  {
      return result;
    }
  },
  getType: function() {
    return this.type;
  },
  getDisplayType: function() {
    var type_string = this.type;
    if (type_string == undefined) return "";
    if (type_string.length > 24) {
      type_string = type_string.substr(0, 25) + "...";
    }
    return type_string;
  },
  deepestInputRank: function() {
    var deepestRank = 0;
    this.inputs.forEach(function(e) {
      if (e.isVisible() && !e.isBackEdge()) {
        if (e.source.rank > deepestRank) {
          deepestRank = e.source.rank;
        }
      }
    });
    return deepestRank;
  },
  areAnyOutputsVisible: function() {
    var visibleCount = 0;
    this.outputs.forEach(function(e) { if (e.isVisible()) ++visibleCount; });
    if (this.outputs.length == visibleCount) return 2;
    if (visibleCount != 0) return 1;
    return 0;
  },
  setOutputVisibility: function(v) {
    var result = false;
    this.outputs.forEach(function(e) {
      e.visible = v;
      if (v) {
        if (!e.target.visible) {
          e.target.visible = true;
          result = true;
        }
      }
    });
    return result;
  },
  setInputVisibility: function(i, v) {
    var edge = this.inputs[i];
    edge.visible = v;
    if (v) {
      if (!edge.source.visible) {
        edge.source.visible = true;
        return true;
      }
    }
    return false;
  },
  getInputApproach: function(index) {
    return this.y - MINIMUM_NODE_INPUT_APPROACH -
      (index % 4) * MINIMUM_EDGE_SEPARATION - DEFAULT_NODE_BUBBLE_RADIUS
  },
  getOutputApproach: function(graph, index) {
    return this.y + this.outputApproach + graph.getNodeHeight(this) +
      + DEFAULT_NODE_BUBBLE_RADIUS;
  },
  getInputX: function(index) {
    var result = this.getTotalNodeWidth() - (NODE_INPUT_WIDTH / 2) +
        (index - this.inputs.length + 1) * NODE_INPUT_WIDTH;
    return result;
  },
  getOutputX: function() {
    return this.getTotalNodeWidth() - (NODE_INPUT_WIDTH / 2);
  },
  getFunctionRelativeSourcePosition: function(graph) {
    return this.pos - graph.sourcePosition;
  },
  hasBackEdges: function() {
    return (this.opcode == "Loop") ||
      ((this.opcode == "Phi" || this.opcode == "EffectPhi") &&
       this.inputs[this.inputs.length - 1].source.opcode == "Loop");
  }
};
