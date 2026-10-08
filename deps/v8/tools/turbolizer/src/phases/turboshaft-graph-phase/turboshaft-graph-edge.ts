// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { TurboshaftGraphOperation } from "./turboshaft-graph-operation";
import { Edge } from "../../edge";
import { TurboshaftGraphBlock } from "./turboshaft-graph-block";
import * as C from "../../common/constants";

export class TurboshaftGraphEdge<Type extends TurboshaftGraphOperation | TurboshaftGraphBlock> extends
  Edge<Type> {
  type: string;

  constructor(target: Type, index: number, source: Type, type: string = "value") {
    super(target, index, source);
    this.type = type;
    this.visible = (target && target.visible) && (source && source.visible);
  }

  public isBackEdge(): boolean {
    if (this.target instanceof TurboshaftGraphBlock) {
      return this.target.hasBackEdges() && (this.target.rank < this.source.rank);
    }
    return this.target.rank < this.source.rank;
  }

  public override isVisible(): boolean {
    if (!super.isVisible()) return false;
    if (this.source instanceof TurboshaftGraphOperation && this.source.block.collapsed) return false;
    if (this.target instanceof TurboshaftGraphOperation && this.target.block.collapsed) return false;
    return true;
  }

  public override generatePath(graph: any, extendHeight: boolean, compactView: boolean = false): string {
    if (this.source instanceof TurboshaftGraphOperation &&
        this.target instanceof TurboshaftGraphOperation &&
        this.type === "control") {

      const source = this.source;
      const target = this.target;

      const sourceBlock = source.block;
      const targetBlock = target.block;

      const exitLeft = targetBlock.x < sourceBlock.x;

      const startX = exitLeft ? source.x : source.x + source.getWidth();
      const startY = source.y + source.getHeight(extendHeight, compactView) / 2;

      const isCatchBlockTarget = targetBlock.exception;
      let endX, endY;
      if (isCatchBlockTarget) {
        const controlInputs = target.inputs.filter(edge => edge.type === "control" && edge.source instanceof TurboshaftGraphOperation);
        const i = controlInputs.indexOf(this as any);
        const total = controlInputs.length;
        const inputX = targetBlock.getWidth() - (C.NODE_INPUT_WIDTH / 2) +
          (i - total + 1) * C.NODE_INPUT_WIDTH;
        endX = targetBlock.x + inputX;
        endY = targetBlock.y - 2 * C.DEFAULT_NODE_BUBBLE_RADIUS - C.ARROW_HEAD_HEIGHT;
      } else {
        endX = target.x + target.getInputX(this.index);
        endY = target.y - 2 * C.DEFAULT_NODE_BUBBLE_RADIUS - C.ARROW_HEAD_HEIGHT;
      }

      const dx = exitLeft ? -50 : 50;
      const dy = 50;

      return `M ${startX} ${startY}\nC ${startX + dx} ${startY},\n${endX} ${endY - dy},\n${endX} ${endY}`;
    }

    return super.generatePath(graph, extendHeight, compactView);
  }
}
