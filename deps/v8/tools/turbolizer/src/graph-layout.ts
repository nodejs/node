// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


import {MAX_RANK_SENTINEL} from "./constants.js"
import {MINIMUM_EDGE_SEPARATION} from "./edge.js"
import {NODE_INPUT_WIDTH, MINIMUM_NODE_OUTPUT_APPROACH, DEFAULT_NODE_BUBBLE_RADIUS} from "./node.js"


const DEFAULT_NODE_ROW_SEPARATION = 130

var traceLayout = false;

function newGraphOccupation(graph) {
  var isSlotFilled = [];
  var maxSlot = 0;
  var minSlot = 0;
  var nodeOccupation = [];

  function slotToIndex(slot) {
    if (slot >= 0) {
      return slot * 2;
    } else {
      return slot * 2 + 1;
    }
  }

  function indexToSlot(index) {
    if ((index % 0) == 0) {
      return index / 2;
    } else {
      return -((index - 1) / 2);
    }
  }

  function positionToSlot(pos) {
    return Math.floor(pos / NODE_INPUT_WIDTH);
  }

  function slotToLeftPosition(slot) {
    return slot * NODE_INPUT_WIDTH
  }

  function slotToRightPosition(slot) {
    return (slot + 1) * NODE_INPUT_WIDTH
  }

  function findSpace(pos, width, direction) {
    var widthSlots = Math.floor((width + NODE_INPUT_WIDTH - 1) /
      NODE_INPUT_WIDTH);
    var currentSlot = positionToSlot(pos + width / 2);
    var currentScanSlot = currentSlot;
    var widthSlotsRemainingLeft = widthSlots;
    var widthSlotsRemainingRight = widthSlots;
    var slotsChecked = 0;
    while (true) {
      var mod = slotsChecked++ % 2;
      currentScanSlot = currentSlot + (mod ? -1 : 1) * (slotsChecked >> 1);
      if (!isSlotFilled[slotToIndex(currentScanSlot)]) {
        if (mod) {
          if (direction <= 0)--widthSlotsRemainingLeft
        } else {
          if (direction >= 0)--widthSlotsRemainingRight
        }
        if (widthSlotsRemainingLeft == 0 ||
          widthSlotsRemainingRight == 0 ||
          (widthSlotsRemainingLeft + widthSlotsRemainingRight) == widthSlots &&
          (widthSlots == slotsChecked)) {
          if (mod) {
            return [currentScanSlot, widthSlots];
          } else {
            return [currentScanSlot - widthSlots + 1, widthSlots];
          }
        }
      } else {
        if (mod) {
          widthSlotsRemainingLeft = widthSlots;
        } else {
          widthSlotsRemainingRight = widthSlots;
        }
      }
    }
  }

  function setIndexRange(from, to, value) {
    if (to < from) {
      throw ("illegal slot range");
    }
    while (from <= to) {
      if (from > maxSlot) {
        maxSlot = from;
      }
      if (from < minSlot) {
        minSlot = from;
      }
      isSlotFilled[slotToIndex(from++)] = value;
    }
  }

  function occupySlotRange(from, to) {
    if (traceLayout) {
      console.log("Occupied [" + slotToLeftPosition(from) + "  " + slotToLeftPosition(to + 1) + ")");
    }
    setIndexRange(from, to, true);
  }

  function clearSlotRange(from, to) {
    if (traceLayout) {
      console.log("Cleared [" + slotToLeftPosition(from) + "  " + slotToLeftPosition(to + 1) + ")");
    }
    setIndexRange(from, to, false);
  }

  function occupyPositionRange(from, to) {
    occupySlotRange(positionToSlot(from), positionToSlot(to - 1));
  }

  function clearPositionRange(from, to) {
    clearSlotRange(positionToSlot(from), positionToSlot(to - 1));
  }

  function occupyPositionRangeWithMargin(from, to, margin) {
    var fromMargin = from - Math.floor(margin);
    var toMargin = to + Math.floor(margin);
    occupyPositionRange(fromMargin, toMargin);
  }

  function clearPositionRangeWithMargin(from, to, margin) {
    var fromMargin = from - Math.floor(margin);
    var toMargin = to + Math.floor(margin);
    clearPositionRange(fromMargin, toMargin);
  }

  var occupation = {
    occupyNodeInputs: function (node) {
      for (var i = 0; i < node.inputs.length; ++i) {
        if (node.inputs[i].isVisible()) {
          var edge = node.inputs[i];
          if (!edge.isBackEdge()) {
            var source = edge.source;
            var horizontalPos = edge.getInputHorizontalPosition(graph);
            if (traceLayout) {
              console.log("Occupying input " + i + " of " + node.id + " at " + horizontalPos);
            }
            occupyPositionRangeWithMargin(horizontalPos,
              horizontalPos,
              NODE_INPUT_WIDTH / 2);
          }
        }
      }
    },
    occupyNode: function (node) {
      var getPlacementHint = function (n) {
        var pos = 0;
        var direction = -1;
        var outputEdges = 0;
        var inputEdges = 0;
        for (var k = 0; k < n.outputs.length; ++k) {
          var outputEdge = n.outputs[k];
          if (outputEdge.isVisible()) {
            var output = n.outputs[k].target;
            for (var l = 0; l < output.inputs.length; ++l) {
              if (output.rank > n.rank) {
                var inputEdge = output.inputs[l];
                if (inputEdge.isVisible()) {
                  ++inputEdges;
                }
                if (output.inputs[l].source == n) {
                  pos += output.x + output.getInputX(l) + NODE_INPUT_WIDTH / 2;
                  outputEdges++;
                  if (l >= (output.inputs.length / 2)) {
                    direction = 1;
                  }
                }
              }
            }
          }
        }
        if (outputEdges != 0) {
          pos = pos / outputEdges;
        }
        if (outputEdges > 1 || inputEdges == 1) {
          direction = 0;
        }
        return [direction, pos];
      }
      var width = node.getTotalNodeWidth();
      var margin = MINIMUM_EDGE_SEPARATION;
      var paddedWidth = width + 2 * margin;
      var placementHint = getPlacementHint(node);
      var x = placementHint[1] - paddedWidth + margin;
      if (traceLayout) {
        console.log("Node " + node.id + " placement hint [" + x + ", " + (x + paddedWidth) + ")");
      }
      var placement = findSpace(x, paddedWidth, placementHint[0]);
      var firstSlot = placement[0];
      var slotWidth = placement[1];
      var endSlotExclusive = firstSlot + slotWidth - 1;
      occupySlotRange(firstSlot, endSlotExclusive);
      nodeOccupation.push([firstSlot, endSlotExclusive]);
      if (placementHint[0] < 0) {
        return slotToLeftPosition(firstSlot + slotWidth) - width - margin;
      } else if (placementHint[0] > 0) {
        return slotToLeftPosition(firstSlot) + margin;
      } else {
        return slotToLeftPosition(firstSlot + slotWidth / 2) - (width / 2);
      }
    },
    clearOccupiedNodes: function () {
      nodeOccupation.forEach(function (o) {
        clearSlotRange(o[0], o[1]);
      });
      nodeOccupation = [];
    },
    clearNodeOutputs: function (source) {
      source.outputs.forEach(function (edge) {
        if (edge.isVisible()) {
          var target = edge.target;
          for (var i = 0; i < target.inputs.length; ++i) {
            if (target.inputs[i].source === source) {
              var horizontalPos = edge.getInputHorizontalPosition(graph);
              clearPositionRangeWithMargin(horizontalPos,
                horizontalPos,
                NODE_INPUT_WIDTH / 2);
            }
          }
        }
      });
    },
    print: function () {
      var s = "";
      for (var currentSlot = -40; currentSlot < 40; ++currentSlot) {
        if (currentSlot != 0) {
          s += " ";
        } else {
          s += "|";
        }
      }
      console.log(s);
      s = "";
      for (var currentSlot2 = -40; currentSlot2 < 40; ++currentSlot2) {
        if (isSlotFilled[slotToIndex(currentSlot2)]) {
          s += "*";
        } else {
          s += " ";
        }
      }
      console.log(s);
    }
  }
  return occupation;
}

export function layoutNodeGraph(graph) {
  // First determine the set of nodes that have no outputs. Those are the
  // basis for bottom-up DFS to determine rank and node placement.
  var endNodesHasNoOutputs = [];
  var startNodesHasNoInputs = [];
  graph.nodes.forEach(function (n, i) {
    endNodesHasNoOutputs[n.id] = true;
    startNodesHasNoInputs[n.id] = true;
  });
  graph.edges.forEach(function (e, i) {
    endNodesHasNoOutputs[e.source.id] = false;
    startNodesHasNoInputs[e.target.id] = false;
  });

  // Finialize the list of start and end nodes.
  var endNodes = [];
  var startNodes = [];
  var visited = [];
  var rank = [];
  graph.nodes.forEach(function (n, i) {
    if (endNodesHasNoOutputs[n.id]) {
      endNodes.push(n);
    }
    if (startNodesHasNoInputs[n.id]) {
      startNodes.push(n);
    }
    visited[n.id] = false;
    rank[n.id] = -1;
    n.rank = 0;
    n.visitOrderWithinRank = 0;
    n.outputApproach = MINIMUM_NODE_OUTPUT_APPROACH;
  });


  var maxRank = 0;
  var visited = [];
  var dfsStack = [];
  var visitOrderWithinRank = 0;

  var worklist = startNodes.slice();
  while (worklist.length != 0) {
    var n = worklist.pop();
    var changed = false;
    if (n.rank == MAX_RANK_SENTINEL) {
      n.rank = 1;
      changed = true;
    }
    var begin = 0;
    var end = n.inputs.length;
    if (n.opcode == 'Phi' || n.opcode == 'EffectPhi') {
      // Keep with merge or loop node
      begin = n.inputs.length - 1;
    } else if (n.hasBackEdges()) {
      end = 1;
    }
    for (var l = begin; l < end; ++l) {
      var input = n.inputs[l].source;
      if (input.visible && input.rank >= n.rank) {
        n.rank = input.rank + 1;
        changed = true;
      }
    }
    if (changed) {
      var hasBackEdges = n.hasBackEdges();
      for (var l = n.outputs.length - 1; l >= 0; --l) {
        if (hasBackEdges && (l != 0)) {
          worklist.unshift(n.outputs[l].target);
        } else {
          worklist.push(n.outputs[l].target);
        }
      }
    }
    if (n.rank > maxRank) {
      maxRank = n.rank;
    }
  }

  visited = [];
  function dfsFindRankLate(n) {
    if (visited[n.id]) return;
    visited[n.id] = true;
    var originalRank = n.rank;
    var newRank = n.rank;
    var firstInput = true;
    for (var l = 0; l < n.outputs.length; ++l) {
      var output = n.outputs[l].target;
      dfsFindRankLate(output);
      var outputRank = output.rank;
      if (output.visible && (firstInput || outputRank <= newRank) &&
        (outputRank > originalRank)) {
        newRank = outputRank - 1;
      }
      firstInput = false;
    }
    if (n.opcode != "Start" && n.opcode != "Phi" && n.opcode != "EffectPhi") {
      n.rank = newRank;
    }
  }

  startNodes.forEach(dfsFindRankLate);

  visited = [];
  function dfsRankOrder(n) {
    if (visited[n.id]) return;
    visited[n.id] = true;
    for (var l = 0; l < n.outputs.length; ++l) {
      var edge = n.outputs[l];
      if (edge.isVisible()) {
        var output = edge.target;
        dfsRankOrder(output);
      }
    }
    if (n.visitOrderWithinRank == 0) {
      n.visitOrderWithinRank = ++visitOrderWithinRank;
    }
  }
  startNodes.forEach(dfsRankOrder);

  endNodes.forEach(function (n) {
    n.rank = maxRank + 1;
  });

  var rankSets = [];
  // Collect sets for each rank.
  graph.nodes.forEach(function (n, i) {
    n.y = n.rank * (DEFAULT_NODE_ROW_SEPARATION + graph.getNodeHeight(n) +
      2 * DEFAULT_NODE_BUBBLE_RADIUS);
    if (n.visible) {
      if (rankSets[n.rank] === undefined) {
        rankSets[n.rank] = [n];
      } else {
        rankSets[n.rank].push(n);
      }
    }
  });

  // Iterate backwards from highest to lowest rank, placing nodes so that they
  // spread out from the "center" as much as possible while still being
  // compact and not overlapping live input lines.
  var occupation = newGraphOccupation(graph);
  var rankCount = 0;

  rankSets.reverse().forEach(function (rankSet) {

    for (var i = 0; i < rankSet.length; ++i) {
      occupation.clearNodeOutputs(rankSet[i]);
    }

    if (traceLayout) {
      console.log("After clearing outputs");
      occupation.print();
    }

    var placedCount = 0;
    rankSet = rankSet.sort(function (a, b) {
      return a.visitOrderWithinRank < b.visitOrderWithinRank;
    });
    for (var i = 0; i < rankSet.length; ++i) {
      var nodeToPlace = rankSet[i];
      if (nodeToPlace.visible) {
        nodeToPlace.x = occupation.occupyNode(nodeToPlace);
        if (traceLayout) {
          console.log("Node " + nodeToPlace.id + " is placed between [" + nodeToPlace.x + ", " + (nodeToPlace.x + nodeToPlace.getTotalNodeWidth()) + ")");
        }
        var staggeredFlooredI = Math.floor(placedCount++ % 3);
        var delta = MINIMUM_EDGE_SEPARATION * staggeredFlooredI
        nodeToPlace.outputApproach += delta;
      } else {
        nodeToPlace.x = 0;
      }
    }

    if (traceLayout) {
      console.log("Before clearing nodes");
      occupation.print();
    }

    occupation.clearOccupiedNodes();

    if (traceLayout) {
      console.log("After clearing nodes");
      occupation.print();
    }

    for (var i = 0; i < rankSet.length; ++i) {
      var node = rankSet[i];
      occupation.occupyNodeInputs(node);
    }

    if (traceLayout) {
      console.log("After occupying inputs");
      occupation.print();
    }

    if (traceLayout) {
      console.log("After determining bounding box");
      occupation.print();
    }
  });

  graph.maxBackEdgeNumber = 0;
  graph.visibleEdges.selectAll("path").each(function (e) {
    if (e.isBackEdge()) {
      e.backEdgeNumber = ++graph.maxBackEdgeNumber;
    } else {
      e.backEdgeNumber = 0;
    }
  });

  redetermineGraphBoundingBox(graph);
}

function redetermineGraphBoundingBox(graph) {
  graph.minGraphX = 0;
  graph.maxGraphNodeX = 1;
  graph.maxGraphX = undefined;  // see below
  graph.minGraphY = 0;
  graph.maxGraphY = 1;

  for (var i = 0; i < graph.nodes.length; ++i) {
    var node = graph.nodes[i];

    if (!node.visible) {
      continue;
    }

    if (node.x < graph.minGraphX) {
      graph.minGraphX = node.x;
    }
    if ((node.x + node.getTotalNodeWidth()) > graph.maxGraphNodeX) {
      graph.maxGraphNodeX = node.x + node.getTotalNodeWidth();
    }
    if ((node.y - 50) < graph.minGraphY) {
      graph.minGraphY = node.y - 50;
    }
    if ((node.y + graph.getNodeHeight(node) + 50) > graph.maxGraphY) {
      graph.maxGraphY = node.y + graph.getNodeHeight(node) + 50;
    }
  }

  graph.maxGraphX = graph.maxGraphNodeX +
    graph.maxBackEdgeNumber * MINIMUM_EDGE_SEPARATION;

  const width = (graph.maxGraphX - graph.minGraphX);
  const height = graph.maxGraphY - graph.minGraphY;
  graph.width = width;
  graph.height = height;

  const extent = [
    [graph.minGraphX - width / 2, graph.minGraphY - height / 2],
    [graph.maxGraphX + width / 2, graph.maxGraphY + height / 2]
  ];
  graph.panZoom.translateExtent(extent);
  graph.minScale();
}
