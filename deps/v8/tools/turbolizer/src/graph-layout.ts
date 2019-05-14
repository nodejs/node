// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { MAX_RANK_SENTINEL } from "../src/constants";
import { MINIMUM_EDGE_SEPARATION, Edge } from "../src/edge";
import { NODE_INPUT_WIDTH, MINIMUM_NODE_OUTPUT_APPROACH, DEFAULT_NODE_BUBBLE_RADIUS, GNode } from "../src/node";
import { Graph } from "./graph";

const DEFAULT_NODE_ROW_SEPARATION = 130;
const traceLayout = false;

function newGraphOccupation(graph: Graph) {
  const isSlotFilled = [];
  let maxSlot = 0;
  let minSlot = 0;
  let nodeOccupation: Array<[number, number]> = [];

  function slotToIndex(slot: number) {
    if (slot >= 0) {
      return slot * 2;
    } else {
      return slot * 2 + 1;
    }
  }

  function positionToSlot(pos: number) {
    return Math.floor(pos / NODE_INPUT_WIDTH);
  }

  function slotToLeftPosition(slot: number) {
    return slot * NODE_INPUT_WIDTH;
  }

  function findSpace(pos: number, width: number, direction: number) {
    const widthSlots = Math.floor((width + NODE_INPUT_WIDTH - 1) /
      NODE_INPUT_WIDTH);
    const currentSlot = positionToSlot(pos + width / 2);
    let currentScanSlot = currentSlot;
    let widthSlotsRemainingLeft = widthSlots;
    let widthSlotsRemainingRight = widthSlots;
    let slotsChecked = 0;
    while (true) {
      const mod = slotsChecked++ % 2;
      currentScanSlot = currentSlot + (mod ? -1 : 1) * (slotsChecked >> 1);
      if (!isSlotFilled[slotToIndex(currentScanSlot)]) {
        if (mod) {
          if (direction <= 0) --widthSlotsRemainingLeft;
        } else {
          if (direction >= 0) --widthSlotsRemainingRight;
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

  function setIndexRange(from: number, to: number, value: boolean) {
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

  function occupySlotRange(from: number, to: number) {
    if (traceLayout) {
      console.log("Occupied [" + slotToLeftPosition(from) + "  " + slotToLeftPosition(to + 1) + ")");
    }
    setIndexRange(from, to, true);
  }

  function clearSlotRange(from: number, to: number) {
    if (traceLayout) {
      console.log("Cleared [" + slotToLeftPosition(from) + "  " + slotToLeftPosition(to + 1) + ")");
    }
    setIndexRange(from, to, false);
  }

  function occupyPositionRange(from: number, to: number) {
    occupySlotRange(positionToSlot(from), positionToSlot(to - 1));
  }

  function clearPositionRange(from: number, to: number) {
    clearSlotRange(positionToSlot(from), positionToSlot(to - 1));
  }

  function occupyPositionRangeWithMargin(from: number, to: number, margin: number) {
    const fromMargin = from - Math.floor(margin);
    const toMargin = to + Math.floor(margin);
    occupyPositionRange(fromMargin, toMargin);
  }

  function clearPositionRangeWithMargin(from: number, to: number, margin: number) {
    const fromMargin = from - Math.floor(margin);
    const toMargin = to + Math.floor(margin);
    clearPositionRange(fromMargin, toMargin);
  }

  const occupation = {
    occupyNodeInputs: function (node: GNode, showTypes: boolean) {
      for (let i = 0; i < node.inputs.length; ++i) {
        if (node.inputs[i].isVisible()) {
          const edge = node.inputs[i];
          if (!edge.isBackEdge()) {
            const horizontalPos = edge.getInputHorizontalPosition(graph, showTypes);
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
    occupyNode: function (node: GNode) {
      const getPlacementHint = function (n: GNode) {
        let pos = 0;
        let direction = -1;
        let outputEdges = 0;
        let inputEdges = 0;
        for (const outputEdge of n.outputs) {
          if (outputEdge.isVisible()) {
            const output = outputEdge.target;
            for (let l = 0; l < output.inputs.length; ++l) {
              if (output.rank > n.rank) {
                const inputEdge = output.inputs[l];
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
      };
      const width = node.getTotalNodeWidth();
      const margin = MINIMUM_EDGE_SEPARATION;
      const paddedWidth = width + 2 * margin;
      const placementHint = getPlacementHint(node);
      const x = placementHint[1] - paddedWidth + margin;
      if (traceLayout) {
        console.log("Node " + node.id + " placement hint [" + x + ", " + (x + paddedWidth) + ")");
      }
      const placement = findSpace(x, paddedWidth, placementHint[0]);
      const firstSlot = placement[0];
      const slotWidth = placement[1];
      const endSlotExclusive = firstSlot + slotWidth - 1;
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
      nodeOccupation.forEach(([firstSlot, endSlotExclusive]) => {
        clearSlotRange(firstSlot, endSlotExclusive);
      });
      nodeOccupation = [];
    },
    clearNodeOutputs: function (source: GNode, showTypes: boolean) {
      source.outputs.forEach(function (edge) {
        if (edge.isVisible()) {
          const target = edge.target;
          for (const inputEdge of target.inputs) {
            if (inputEdge.source === source) {
              const horizontalPos = edge.getInputHorizontalPosition(graph, showTypes);
              clearPositionRangeWithMargin(horizontalPos,
                horizontalPos,
                NODE_INPUT_WIDTH / 2);
            }
          }
        }
      });
    },
    print: function () {
      let s = "";
      for (let currentSlot = -40; currentSlot < 40; ++currentSlot) {
        if (currentSlot != 0) {
          s += " ";
        } else {
          s += "|";
        }
      }
      console.log(s);
      s = "";
      for (let currentSlot2 = -40; currentSlot2 < 40; ++currentSlot2) {
        if (isSlotFilled[slotToIndex(currentSlot2)]) {
          s += "*";
        } else {
          s += " ";
        }
      }
      console.log(s);
    }
  };
  return occupation;
}

export function layoutNodeGraph(graph: Graph, showTypes: boolean): void {
  // First determine the set of nodes that have no outputs. Those are the
  // basis for bottom-up DFS to determine rank and node placement.

  const start = performance.now();

  const endNodesHasNoOutputs = [];
  const startNodesHasNoInputs = [];
  for (const n of graph.nodes()) {
    endNodesHasNoOutputs[n.id] = true;
    startNodesHasNoInputs[n.id] = true;
  }
  graph.forEachEdge((e: Edge) => {
    endNodesHasNoOutputs[e.source.id] = false;
    startNodesHasNoInputs[e.target.id] = false;
  });

  // Finialize the list of start and end nodes.
  const endNodes: Array<GNode> = [];
  const startNodes: Array<GNode> = [];
  let visited: Array<boolean> = [];
  const rank: Array<number> = [];
  for (const n of graph.nodes()) {
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
  }

  if (traceLayout) {
    console.log(`layoutGraph init ${performance.now() - start}`);
  }

  let maxRank = 0;
  visited = [];
  let visitOrderWithinRank = 0;

  const worklist: Array<GNode> = startNodes.slice();
  while (worklist.length != 0) {
    const n: GNode = worklist.pop();
    let changed = false;
    if (n.rank == MAX_RANK_SENTINEL) {
      n.rank = 1;
      changed = true;
    }
    let begin = 0;
    let end = n.inputs.length;
    if (n.nodeLabel.opcode == 'Phi' ||
      n.nodeLabel.opcode == 'EffectPhi' ||
      n.nodeLabel.opcode == 'InductionVariablePhi') {
      // Keep with merge or loop node
      begin = n.inputs.length - 1;
    } else if (n.hasBackEdges()) {
      end = 1;
    }
    for (let l = begin; l < end; ++l) {
      const input = n.inputs[l].source;
      if (input.visible && input.rank >= n.rank) {
        n.rank = input.rank + 1;
        changed = true;
      }
    }
    if (changed) {
      const hasBackEdges = n.hasBackEdges();
      for (let l = n.outputs.length - 1; l >= 0; --l) {
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

  if (traceLayout) {
    console.log(`layoutGraph worklist ${performance.now() - start}`);
  }

  visited = [];
  function dfsFindRankLate(n: GNode) {
    if (visited[n.id]) return;
    visited[n.id] = true;
    const originalRank = n.rank;
    let newRank = n.rank;
    let isFirstInput = true;
    for (const outputEdge of n.outputs) {
      const output = outputEdge.target;
      dfsFindRankLate(output);
      const outputRank = output.rank;
      if (output.visible && (isFirstInput || outputRank <= newRank) &&
        (outputRank > originalRank)) {
        newRank = outputRank - 1;
      }
      isFirstInput = false;
    }
    if (n.nodeLabel.opcode != "Start" && n.nodeLabel.opcode != "Phi" && n.nodeLabel.opcode != "EffectPhi" && n.nodeLabel.opcode != "InductionVariablePhi") {
      n.rank = newRank;
    }
  }

  startNodes.forEach(dfsFindRankLate);

  visited = [];
  function dfsRankOrder(n: GNode) {
    if (visited[n.id]) return;
    visited[n.id] = true;
    for (const outputEdge of n.outputs) {
      if (outputEdge.isVisible()) {
        const output = outputEdge.target;
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

  const rankSets: Array<Array<GNode>> = [];
  // Collect sets for each rank.
  for (const n of graph.nodes()) {
    n.y = n.rank * (DEFAULT_NODE_ROW_SEPARATION + n.getNodeHeight(showTypes) +
      2 * DEFAULT_NODE_BUBBLE_RADIUS);
    if (n.visible) {
      if (rankSets[n.rank] === undefined) {
        rankSets[n.rank] = [n];
      } else {
        rankSets[n.rank].push(n);
      }
    }
  }

  // Iterate backwards from highest to lowest rank, placing nodes so that they
  // spread out from the "center" as much as possible while still being
  // compact and not overlapping live input lines.
  const occupation = newGraphOccupation(graph);

  rankSets.reverse().forEach(function (rankSet: Array<GNode>) {

    for (const node of rankSet) {
      occupation.clearNodeOutputs(node, showTypes);
    }

    if (traceLayout) {
      console.log("After clearing outputs");
      occupation.print();
    }

    let placedCount = 0;
    rankSet = rankSet.sort((a: GNode, b: GNode) => {
      if (a.visitOrderWithinRank < b.visitOrderWithinRank) {
        return -1;
      } else if (a.visitOrderWithinRank == b.visitOrderWithinRank) {
        return 0;
      } else {
        return 1;
      }
    });

    for (const nodeToPlace of rankSet) {
      if (nodeToPlace.visible) {
        nodeToPlace.x = occupation.occupyNode(nodeToPlace);
        if (traceLayout) {
          console.log("Node " + nodeToPlace.id + " is placed between [" + nodeToPlace.x + ", " + (nodeToPlace.x + nodeToPlace.getTotalNodeWidth()) + ")");
        }
        const staggeredFlooredI = Math.floor(placedCount++ % 3);
        const delta = MINIMUM_EDGE_SEPARATION * staggeredFlooredI;
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

    for (const node of rankSet) {
      occupation.occupyNodeInputs(node, showTypes);
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
  graph.forEachEdge((e: Edge) => {
    if (e.isBackEdge()) {
      e.backEdgeNumber = ++graph.maxBackEdgeNumber;
    } else {
      e.backEdgeNumber = 0;
    }
  });
}
