// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { Edge } from "../../edge";
import { GraphNode } from "./graph-node";

export class GraphEdge extends Edge<GraphNode> {
  type: string;

  constructor(target: GraphNode, index: number, source: GraphNode, type: string) {
    super(target, index, source);
    this.type = type;
  }

  public isBackEdge(): boolean {
    return this.target.hasBackEdges() && (this.target.rank < this.source.rank);
  }
}
