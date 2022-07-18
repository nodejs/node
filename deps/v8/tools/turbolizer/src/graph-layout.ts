// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as C from "./common/constants";
import { Graph } from "./graph";
import { GraphNode } from "./phases/graph-phase/graph-node";
import { GraphEdge } from "./phases/graph-phase/graph-edge";
import { GraphStateType } from "./phases/graph-phase/graph-phase";

export class GraphLayout {
  graph: Graph;
  graphOccupation: GraphOccupation;
  startTime: number;
  maxRank: number;
  visitOrderWithinRank: number;

  constructor(graph: Graph) {
    this.graph = graph;
    this.graphOccupation = new GraphOccupation(graph);
    this.maxRank = 0;
    this.visitOrderWithinRank = 0;
  }

  public rebuild(showTypes: boolean): void {
    switch (this.graph.graphPhase.stateType) {
      case GraphStateType.NeedToFullRebuild:
        this.fullRebuild(showTypes);
        break;
      case GraphStateType.Cached:
        this.cachedRebuild();
        break;
      default:
        throw "Unsupported graph state type";
    }
    this.graph.graphPhase.rendered = true;
  }

  private fullRebuild(showTypes: boolean): void {
    this.startTime = performance.now();
    this.maxRank = 0;
    this.visitOrderWithinRank = 0;

    const [startNodes, endNodes] = this.initNodes();
    this.initWorkList(startNodes);

    let visited = new Array<boolean>();
    startNodes.forEach((sn: GraphNode) => this.dfsFindRankLate(visited, sn));
    visited = new Array<boolean>();
    startNodes.forEach((sn: GraphNode) => this.dfsRankOrder(visited, sn));
    endNodes.forEach((node: GraphNode) => node.rank = this.maxRank + 1);

    const rankSets = this.getRankSets(showTypes);
    this.placeNodes(rankSets, showTypes);
    this.calculateBackEdgeNumbers();
    this.graph.graphPhase.stateType = GraphStateType.Cached;
  }

  private cachedRebuild(): void {
    this.calculateBackEdgeNumbers();
  }

  private initNodes(): [Array<GraphNode>, Array<GraphNode>] {
    // First determine the set of nodes that have no outputs. Those are the
    // basis for bottom-up DFS to determine rank and node placement.
    const endNodesHasNoOutputs = new Array<boolean>();
    const startNodesHasNoInputs = new Array<boolean>();
    for (const node of this.graph.nodes()) {
      endNodesHasNoOutputs[node.id] = true;
      startNodesHasNoInputs[node.id] = true;
    }
    this.graph.forEachEdge((edge: GraphEdge) => {
      endNodesHasNoOutputs[edge.source.id] = false;
      startNodesHasNoInputs[edge.target.id] = false;
    });

    // Finialize the list of start and end nodes.
    const endNodes = new Array<GraphNode>();
    const startNodes = new Array<GraphNode>();
    const visited = new Array<boolean>();
    for (const node of this.graph.nodes()) {
      if (endNodesHasNoOutputs[node.id]) {
        endNodes.push(node);
      }
      if (startNodesHasNoInputs[node.id]) {
        startNodes.push(node);
      }
      visited[node.id] = false;
      node.rank = 0;
      node.visitOrderWithinRank = 0;
      node.outputApproach = C.MINIMUM_NODE_OUTPUT_APPROACH;
    }
    this.trace("layoutGraph init");
    return [startNodes, endNodes];
  }

  private initWorkList(startNodes: Array<GraphNode>): void {
    const workList = startNodes.slice();
    while (workList.length != 0) {
      const node = workList.pop();
      let changed = false;
      if (node.rank == C.MAX_RANK_SENTINEL) {
        node.rank = 1;
        changed = true;
      }
      let begin = 0;
      let end = node.inputs.length;
      if (node.nodeLabel.opcode === "Phi" ||
        node.nodeLabel.opcode === "EffectPhi" ||
        node.nodeLabel.opcode === "InductionVariablePhi") {
        // Keep with merge or loop node
        begin = node.inputs.length - 1;
      } else if (node.hasBackEdges()) {
        end = 1;
      }
      for (let l = begin; l < end; ++l) {
        const input = node.inputs[l].source;
        if (input.visible && input.rank >= node.rank) {
          node.rank = input.rank + 1;
          changed = true;
        }
      }
      if (changed) {
        const hasBackEdges = node.hasBackEdges();
        for (let l = node.outputs.length - 1; l >= 0; --l) {
          if (hasBackEdges && (l != 0)) {
            workList.unshift(node.outputs[l].target);
          } else {
            workList.push(node.outputs[l].target);
          }
        }
      }
      this.maxRank = Math.max(node.rank, this.maxRank);
    }
    this.trace("layoutGraph work list");
  }

  private dfsFindRankLate(visited: Array<boolean>, node: GraphNode): void {
    if (visited[node.id]) return;
    visited[node.id] = true;
    const originalRank = node.rank;
    let newRank = node.rank;
    let isFirstInput = true;
    for (const outputEdge of node.outputs) {
      const output = outputEdge.target;
      this.dfsFindRankLate(visited, output);
      const outputRank = output.rank;
      if (output.visible && (isFirstInput || outputRank <= newRank) &&
        (outputRank > originalRank)) {
        newRank = outputRank - 1;
      }
      isFirstInput = false;
    }
    if (node.nodeLabel.opcode !== "Start" && node.nodeLabel.opcode !== "Phi"
      && node.nodeLabel.opcode !== "EffectPhi"
      && node.nodeLabel.opcode !== "InductionVariablePhi") {
      node.rank = newRank;
    }
  }

  private dfsRankOrder(visited: Array<boolean>, node: GraphNode): void {
    if (visited[node.id]) return;
    visited[node.id] = true;
    for (const outputEdge of node.outputs) {
      if (outputEdge.isVisible()) {
        const output = outputEdge.target;
        this.dfsRankOrder(visited, output);
      }
    }
    if (node.visitOrderWithinRank == 0) {
      node.visitOrderWithinRank = ++this.visitOrderWithinRank;
    }
  }

  private getRankSets(showTypes: boolean): Array<Array<GraphNode>> {
    const rankSets = new Array<Array<GraphNode>>();
    for (const node of this.graph.nodes()) {
      node.y = node.rank * (C.DEFAULT_NODE_ROW_SEPARATION +
        node.getHeight(showTypes) + 2 * C.DEFAULT_NODE_BUBBLE_RADIUS);
      if (node.visible) {
        if (!rankSets[node.rank]) {
          rankSets[node.rank] = new Array<GraphNode>(node);
        } else {
          rankSets[node.rank].push(node);
        }
      }
    }
    return rankSets;
  }

  private placeNodes(rankSets: Array<Array<GraphNode>>, showTypes: boolean): void {
    // Iterate backwards from highest to lowest rank, placing nodes so that they
    // spread out from the "center" as much as possible while still being
    // compact and not overlapping live input lines.
    rankSets.reverse().forEach((rankSet: Array<GraphNode>) => {
      for (const node of rankSet) {
        this.graphOccupation.clearNodeOutputs(node, showTypes);
      }

      this.traceOccupation("After clearing outputs");

      let placedCount = 0;
      rankSet = rankSet.sort((a: GraphNode, b: GraphNode) => a.compare(b));
      for (const node of rankSet) {
        if (node.visible) {
          node.x = this.graphOccupation.occupyNode(node);
          this.trace(`Node ${node.id} is placed between [${node.x}, ${node.x + node.getWidth()})`);
          const staggeredFlooredI = Math.floor(placedCount++ % 3);
          const delta = C.MINIMUM_EDGE_SEPARATION * staggeredFlooredI;
          node.outputApproach += delta;
        } else {
          node.x = 0;
        }
      }

      this.traceOccupation("Before clearing nodes");

      this.graphOccupation.clearOccupiedNodes();

      this.traceOccupation("After clearing nodes");

      for (const node of rankSet) {
        this.graphOccupation.occupyNodeInputs(node, showTypes);
      }

      this.traceOccupation("After occupying inputs and determining bounding box");
    });
  }

  private calculateBackEdgeNumbers(): void {
    this.graph.maxBackEdgeNumber = 0;
    this.graph.forEachEdge((edge: GraphEdge) => {
      if (edge.isBackEdge()) {
        edge.backEdgeNumber = ++this.graph.maxBackEdgeNumber;
      } else {
        edge.backEdgeNumber = 0;
      }
    });
  }

  private trace(message: string): void {
    if (C.TRACE_LAYOUT) {
      console.log(`${message} ${performance.now() - this.startTime}`);
    }
  }

  private traceOccupation(message: string): void {
    if (C.TRACE_LAYOUT) {
      console.log(message);
      this.graphOccupation.print();
    }
  }
}

class GraphOccupation {
  graph: Graph;
  filledSlots: Array<boolean>;
  nodeOccupations: Array<[number, number]>;
  minSlot: number;
  maxSlot: number;

  constructor(graph: Graph) {
    this.graph = graph;
    this.filledSlots = new Array<boolean>();
    this.nodeOccupations = new Array<[number, number]>();
    this.minSlot = 0;
    this.maxSlot = 0;
  }

  public clearNodeOutputs(source: GraphNode, showTypes: boolean): void {
    for (const edge of source.outputs) {
      if (!edge.isVisible()) continue;
      for (const inputEdge of edge.target.inputs) {
        if (inputEdge.source === source) {
          const horizontalPos = edge.getInputHorizontalPosition(this.graph, showTypes);
          this.clearPositionRangeWithMargin(horizontalPos, horizontalPos, C.NODE_INPUT_WIDTH / 2);
        }
      }
    }
  }

  public clearOccupiedNodes(): void {
    for (const [firstSlot, endSlotExclusive] of this.nodeOccupations) {
      this.clearSlotRange(firstSlot, endSlotExclusive);
    }
    this.nodeOccupations = new Array<[number, number]>();
  }

  public occupyNode(node: GraphNode): number {
    const width = node.getWidth();
    const margin = C.MINIMUM_EDGE_SEPARATION;
    const paddedWidth = width + 2 * margin;
    const [direction, position] = this.getPlacementHint(node);
    const x = position - paddedWidth + margin;
    this.trace(`Node ${node.id} placement hint [${x}, ${(x + paddedWidth)})`);
    const placement = this.findSpace(x, paddedWidth, direction);
    const [firstSlot, slotWidth] = placement;
    const endSlotExclusive = firstSlot + slotWidth - 1;
    this.occupySlotRange(firstSlot, endSlotExclusive);
    this.nodeOccupations.push([firstSlot, endSlotExclusive]);
    if (direction < 0) {
      return this.slotToLeftPosition(firstSlot + slotWidth) - width - margin;
    } else if (direction > 0) {
      return this.slotToLeftPosition(firstSlot) + margin;
    } else {
      return this.slotToLeftPosition(firstSlot + slotWidth / 2) - (width / 2);
    }
  }

  public occupyNodeInputs(node: GraphNode, showTypes: boolean): void {
    for (let i = 0; i < node.inputs.length; ++i) {
      if (node.inputs[i].isVisible()) {
        const edge = node.inputs[i];
        if (!edge.isBackEdge()) {
          const horizontalPos = edge.getInputHorizontalPosition(this.graph, showTypes);
          this.trace(`Occupying input ${i} of ${node.id} at ${horizontalPos}`);
          this.occupyPositionRangeWithMargin(horizontalPos, horizontalPos, C.NODE_INPUT_WIDTH / 2);
        }
      }
    }
  }

  public print(): void {
    let output = "";
    for (let currentSlot = -40; currentSlot < 40; ++currentSlot) {
      if (currentSlot != 0) {
        output += " ";
      } else {
        output += "|";
      }
    }
    console.log(output);
    output = "";
    for (let currentSlot2 = -40; currentSlot2 < 40; ++currentSlot2) {
      if (this.filledSlots[this.slotToIndex(currentSlot2)]) {
        output += "*";
      } else {
        output += " ";
      }
    }
    console.log(output);
  }

  private getPlacementHint(node: GraphNode): [number, number] {
    let position = 0;
    let direction = -1;
    let outputEdges = 0;
    let inputEdges = 0;
    for (const outputEdge of node.outputs) {
      if (!outputEdge.isVisible()) continue;
      const output = outputEdge.target;
      for (let l = 0; l < output.inputs.length; ++l) {
        if (output.rank > node.rank) {
          const inputEdge = output.inputs[l];
          if (inputEdge.isVisible()) ++inputEdges;
          if (output.inputs[l].source == node) {
            position += output.x + output.getInputX(l) + C.NODE_INPUT_WIDTH / 2;
            outputEdges++;
            if (l >= (output.inputs.length / 2)) {
              direction = 1;
            }
          }
        }
      }
    }
    if (outputEdges != 0) {
      position /= outputEdges;
    }
    if (outputEdges > 1 || inputEdges == 1) {
      direction = 0;
    }
    return [direction, position];
  }

  private occupyPositionRange(from: number, to: number): void {
    this.occupySlotRange(this.positionToSlot(from), this.positionToSlot(to - 1));
  }

  private clearPositionRange(from: number, to: number): void {
    this.clearSlotRange(this.positionToSlot(from), this.positionToSlot(to - 1));
  }

  private occupySlotRange(from: number, to: number): void {
    this.trace(`Occupied [${this.slotToLeftPosition(from)} ${this.slotToLeftPosition(to + 1)})`);
    this.setIndexRange(from, to, true);
  }

  private clearSlotRange(from: number, to: number): void {
    this.trace(`Cleared [${this.slotToLeftPosition(from)} ${this.slotToLeftPosition(to + 1)})`);
    this.setIndexRange(from, to, false);
  }

  private clearPositionRangeWithMargin(from: number, to: number, margin: number): void {
    const fromMargin = from - Math.floor(margin);
    const toMargin = to + Math.floor(margin);
    this.clearPositionRange(fromMargin, toMargin);
  }

  private occupyPositionRangeWithMargin(from: number, to: number, margin: number): void {
    const fromMargin = from - Math.floor(margin);
    const toMargin = to + Math.floor(margin);
    this.occupyPositionRange(fromMargin, toMargin);
  }

  private findSpace(pos: number, width: number, direction: number): [number, number] {
    const widthSlots = Math.floor((width + C.NODE_INPUT_WIDTH - 1) /
      C.NODE_INPUT_WIDTH);

    const currentSlot = this.positionToSlot(pos + width / 2);
    let widthSlotsRemainingLeft = widthSlots;
    let widthSlotsRemainingRight = widthSlots;
    let slotsChecked = 0;

    while (true) {
      const mod = slotsChecked++ % 2;
      const currentScanSlot = currentSlot + (mod ? -1 : 1) * (slotsChecked >> 1);
      if (!this.filledSlots[this.slotToIndex(currentScanSlot)]) {
        if (mod) {
          if (direction <= 0) --widthSlotsRemainingLeft;
        } else if (direction >= 0) {
          --widthSlotsRemainingRight;
        }
        if (widthSlotsRemainingLeft == 0 || widthSlotsRemainingRight == 0 ||
          (widthSlotsRemainingLeft + widthSlotsRemainingRight) == widthSlots &&
          (widthSlots == slotsChecked)) {
          return mod ? [currentScanSlot, widthSlots]
                     : [currentScanSlot - widthSlots + 1, widthSlots];
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

  private setIndexRange(from: number, to: number, value: boolean): void {
    if (to < from) throw ("Illegal slot range");
    while (from <= to) {
      this.maxSlot = Math.max(from, this.maxSlot);
      this.minSlot = Math.min(from, this.minSlot);
      this.filledSlots[this.slotToIndex(from++)] = value;
    }
  }

  private positionToSlot(position: number): number {
    return Math.floor(position / C.NODE_INPUT_WIDTH);
  }

  private slotToIndex(slot: number): number {
    return slot >= 0 ? slot * 2 : slot * 2 + 1;
  }

  private slotToLeftPosition(slot: number): number {
    return slot * C.NODE_INPUT_WIDTH;
  }

  private trace(message): void {
    if (C.TRACE_LAYOUT) {
      console.log(message);
    }
  }
}
