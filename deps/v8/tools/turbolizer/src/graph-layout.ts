// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as C from "./common/constants";
import { Graph } from "./graph";
import { GraphNode } from "./phases/graph-phase/graph-node";
import { GraphEdge } from "./phases/graph-phase/graph-edge";
import { GraphStateType } from "./phases/graph-phase/graph-phase";
import { LayoutOccupation } from "./layout-occupation";

export class GraphLayout {
  graph: Graph;
  layoutOccupation: LayoutOccupation;
  startTime: number;
  maxRank: number;
  visitOrderWithinRank: number;

  constructor(graph: Graph) {
    this.graph = graph;
    this.layoutOccupation = new LayoutOccupation(graph);
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
        this.layoutOccupation.clearOutputs(node, showTypes);
      }

      this.traceOccupation("After clearing outputs");

      let placedCount = 0;
      rankSet = rankSet.sort((a: GraphNode, b: GraphNode) => a.compare(b));
      for (const node of rankSet) {
        if (node.visible) {
          node.x = this.layoutOccupation.occupy(node);
          this.trace(`Node ${node.id} is placed between [${node.x}, ${node.x + node.getWidth()})`);
          const staggeredFlooredI = Math.floor(placedCount++ % 3);
          const delta = C.MINIMUM_EDGE_SEPARATION * staggeredFlooredI;
          node.outputApproach += delta;
        } else {
          node.x = 0;
        }
      }

      this.traceOccupation("Before clearing nodes");

      this.layoutOccupation.clearOccupied();

      this.traceOccupation("After clearing nodes");

      for (const node of rankSet) {
        this.layoutOccupation.occupyInputs(node, showTypes);
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
      this.layoutOccupation.print();
    }
  }
}
