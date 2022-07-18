// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { TurboshaftGraphNode } from "./turboshaft-graph-node";
import { Node } from "../../node";
import { TurboshaftGraphEdge } from "./turboshaft-graph-edge";

export class TurboshaftGraphBlock extends Node<TurboshaftGraphEdge<TurboshaftGraphBlock>> {
  type: TurboshaftGraphBlockType;
  deferred: boolean;
  predecessors: Array<string>;
  nodes: Array<TurboshaftGraphNode>;

  constructor(id: number, type: TurboshaftGraphBlockType, deferred: boolean,
              predecessors: Array<string>) {
    super(id, `${type} ${id}${deferred ? " (deferred)" : ""}`);
    this.type = type;
    this.deferred = deferred;
    this.predecessors = predecessors ?? new Array<string>();
    this.nodes = new Array<TurboshaftGraphNode>();
    this.visible = true;
  }

  public getHeight(showProperties: boolean): number {
    return this.nodes.reduce<number>((accumulator: number, node: TurboshaftGraphNode) => {
      return accumulator + node.getHeight(showProperties);
    }, this.labelBox.height);
  }

  public getWidth(): number {
    const maxWidth = Math.max(...this.nodes.map((node: TurboshaftGraphNode) => node.getWidth()));
    return Math.max(maxWidth, this.labelBox.width) + 50;
  }

  public toString(): string {
    return `B${this.id}`;
  }
}

export enum TurboshaftGraphBlockType {
  Loop = "LOOP",
  Merge = "MERGE",
  Block = "BLOCK"
}
