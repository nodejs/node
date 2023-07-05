// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { Phase, PhaseType } from "../phase";
import { NodeLabel } from "../../node-label";
import { BytecodeOrigin, NodeOrigin } from "../../origin";
import { GraphNode } from "./graph-node";
import { GraphEdge } from "./graph-edge";
import { Source } from "../../source";
import { InstructionsPhase } from "../instructions-phase";
import {
  BytecodePosition,
  InliningPosition,
  PositionsContainer,
  SourcePosition
} from "../../position";

export class GraphPhase extends Phase {
  data: GraphData;
  stateType: GraphStateType;
  instructionsPhase: InstructionsPhase;
  nodeIdToNodeMap: Array<GraphNode>;
  originIdToNodesMap: Map<string, Array<GraphNode>>;
  positions: PositionsContainer;
  highestNodeId: number;
  rendered: boolean;
  transform: { x: number, y: number, scale: number };

  constructor(name: string, dataJson, nodeMap: Array<GraphNode>, sources: Array<Source>,
              inlinings: Array<InliningPosition>) {
    super(name, PhaseType.Graph);
    this.data = new GraphData();
    this.stateType = GraphStateType.NeedToFullRebuild;
    this.instructionsPhase = new InstructionsPhase();
    this.nodeIdToNodeMap = new Array<GraphNode>();
    this.originIdToNodesMap = new Map<string, Array<GraphNode>>();
    this.positions = new PositionsContainer();
    this.highestNodeId = 0;
    this.rendered = false;
    this.parseDataFromJSON(dataJson, nodeMap, sources, inlinings);
  }

  private parseDataFromJSON(dataJson, nodeMap: Array<GraphNode>, sources: Array<Source>,
                            inlinings: Array<InliningPosition>): void {
    this.data = new GraphData();
    this.parseNodesFromJSON(dataJson.nodes, nodeMap, sources, inlinings);
    this.parseEdgesFromJSON(dataJson.edges);
  }

  private parseNodesFromJSON(nodesJSON, nodeMap: Array<GraphNode>, sources: Array<Source>,
                             inlinings: Array<InliningPosition>): void {
    for (const node of nodesJSON) {
      let sourcePosition: SourcePosition = null;
      const sourcePositionJson = node.sourcePosition;
      if (sourcePositionJson) {
        const scriptOffset = sourcePositionJson.scriptOffset;
        const inliningId = sourcePositionJson.inliningId;
        sourcePosition = new SourcePosition(scriptOffset, inliningId);
      }

      let origin: NodeOrigin | BytecodeOrigin = null;
      let bytecodePosition: BytecodePosition = null;
      const originJson = node.origin;
      if (originJson) {
        const nodeId = originJson.nodeId;
        if (nodeId) {
          origin = new NodeOrigin(nodeId, nodeMap[nodeId], originJson.phase, originJson.reducer);
          bytecodePosition = nodeMap[nodeId]?.nodeLabel.bytecodePosition;
        } else {
          origin = new BytecodeOrigin(originJson.bytecodePosition, originJson.phase,
            originJson.reducer);
          const inliningId = sourcePosition ? sourcePosition.inliningId : -1;
          bytecodePosition = new BytecodePosition(originJson.bytecodePosition, inliningId);
        }
      }

      const label = new NodeLabel(node.id, node.label, node.title, node.live, node.properties,
        sourcePosition, bytecodePosition, origin, node.opcode, node.control, node.opinfo,
        node.type);

      const newNode = new GraphNode(label);
      this.data.nodes.push(newNode);
      this.highestNodeId = Math.max(this.highestNodeId, newNode.id);
      this.nodeIdToNodeMap[newNode.identifier()] = newNode;

      const previous = nodeMap[newNode.id];
      if (!newNode.equals(previous)) {
        if (previous) newNode.nodeLabel.setInplaceUpdatePhase(this.name);
        nodeMap[newNode.id] = newNode;
      }

      if (origin && origin instanceof NodeOrigin) {
        const identifier = origin.identifier();
        if (!this.originIdToNodesMap.has(identifier)) {
          this.originIdToNodesMap.set(identifier, new Array<GraphNode>());
        }
        this.originIdToNodesMap.get(identifier).push(newNode);
      }

      if (sourcePosition) {
        const inlining = inlinings[sourcePosition.inliningId];
        if (inlining) sources[inlining.sourceId].sourcePositions.push(sourcePosition);
        this.positions.addSourcePosition(newNode.identifier(), sourcePosition);
      }

      if (bytecodePosition) {
        this.positions.addBytecodePosition(newNode.identifier(), bytecodePosition);
      }
    }
  }

  private parseEdgesFromJSON(edgesJSON): void {
    for (const edge of edgesJSON) {
      const target = this.nodeIdToNodeMap[edge.target];
      const source = this.nodeIdToNodeMap[edge.source];
      const newEdge = new GraphEdge(target, edge.index, source, edge.type);
      this.data.edges.push(newEdge);
      target.inputs.push(newEdge);
      source.outputs.push(newEdge);
      if (edge.type === "control") {
        source.cfg = true;
      }
    }
  }
}

export class GraphData {
  nodes: Array<GraphNode>;
  edges: Array<GraphEdge>;

  constructor() {
    this.nodes = new Array<GraphNode>();
    this.edges = new Array<GraphEdge>();
  }
}

export enum GraphStateType {
  NeedToFullRebuild,
  Cached
}
