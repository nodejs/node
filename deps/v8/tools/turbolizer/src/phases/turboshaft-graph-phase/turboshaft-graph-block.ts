// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as C from "../../common/constants";
import { measureText } from "../../common/util";
import { TurboshaftGraphNode } from "./turboshaft-graph-node";
import { Node } from "../../node";
import { TurboshaftGraphEdge } from "./turboshaft-graph-edge";

export class TurboshaftGraphBlock extends Node<TurboshaftGraphEdge<TurboshaftGraphBlock>> {
  type: TurboshaftGraphBlockType;
  deferred: boolean;
  predecessors: Array<string>;
  nodes: Array<TurboshaftGraphNode>;
  showCustomData: boolean;
  collapsed: boolean;
  collapsedLabel: string;
  collapsedLabelBox: { width: number, height: number };
  width: number;
  height: number;

  constructor(id: number, type: TurboshaftGraphBlockType, deferred: boolean,
              predecessors: Array<string>) {
    super(id, `${type} ${id}${deferred ? " (deferred)" : ""}`);
    this.type = type;
    this.deferred = deferred;
    this.predecessors = predecessors ?? new Array<string>();
    this.nodes = new Array<TurboshaftGraphNode>();
    this.visible = true;
  }

  public getHeight(showCustomData: boolean): number {
    if (this.collapsed) return this.labelBox.height + this.collapsedLabelBox.height;

    if (this.showCustomData != showCustomData) {
      this.height = this.nodes.reduce<number>((accumulator: number, node: TurboshaftGraphNode) => {
        return accumulator + node.getHeight(showCustomData);
      }, this.labelBox.height);
      this.showCustomData = showCustomData;
    }

    return this.height;
  }

  public getWidth(): number {
    if (!this.width) {
      const labelWidth = this.labelBox.width + this.labelBox.height
        + C.TURBOSHAFT_COLLAPSE_ICON_X_INDENT;
      const maxNodesWidth = Math.max(...this.nodes.map((node: TurboshaftGraphNode) =>
        node.getWidth()));
      this.width = Math.max(maxNodesWidth, labelWidth, this.collapsedLabelBox.width)
        + C.TURBOSHAFT_NODE_X_INDENT * 2;
    }
    return this.width;
  }

  public compressHeight(): void {
    if (this.collapsed) {
      this.height = this.getHeight(null);
      this.showCustomData = null;
    }
  }

  public getRankIndent() {
    return this.rank * (C.TURBOSHAFT_BLOCK_ROW_SEPARATION + 2 * C.DEFAULT_NODE_BUBBLE_RADIUS);
  }

  public initCollapsedLabel() {
    this.collapsedLabel = `${this.nodes.length} operations`;
    this.collapsedLabelBox = measureText(this.collapsedLabel);
  }

  public hasBackEdges(): boolean {
    return (this.type == TurboshaftGraphBlockType.Loop) ||
      (this.type == TurboshaftGraphBlockType.Merge &&
        this.inputs.length > 0 &&
        this.inputs[this.inputs.length - 1].source.type == TurboshaftGraphBlockType.Loop);
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
