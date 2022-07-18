// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { GraphNode } from "./phases/graph-phase/graph-node";
import { TurboshaftGraphNode } from "./phases/turboshaft-graph-phase/turboshaft-graph-node";
import { TurboshaftGraphBlock } from "./phases/turboshaft-graph-phase/turboshaft-graph-block";

export abstract class Edge<NodeType extends GraphNode | TurboshaftGraphNode
  | TurboshaftGraphBlock> {
  target: NodeType;
  source: NodeType;
  backEdgeNumber: number;
  visible: boolean;

  constructor(target: NodeType, source: NodeType) {
    this.target = target;
    this.source = source;
    this.backEdgeNumber = 0;
    this.visible = false;
  }

  public isVisible(): boolean {
    return this.visible && this.source.visible && this.target.visible;
  }
}
