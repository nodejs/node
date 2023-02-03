// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as C from "../../common/constants";
import { measureText } from "../../common/util";
import { TurboshaftGraphEdge } from "./turboshaft-graph-edge";
import { TurboshaftGraphBlock } from "./turboshaft-graph-block";
import { Node } from "../../node";
import { BytecodePosition, SourcePosition } from "../../position";
import { NodeOrigin } from "../../origin";

export class TurboshaftGraphNode extends Node<TurboshaftGraphEdge<TurboshaftGraphNode>> {
  title: string;
  block: TurboshaftGraphBlock;
  sourcePosition: SourcePosition;
  bytecodePosition: BytecodePosition;
  origin: NodeOrigin;
  opPropertiesType: OpPropertiesType;

  constructor(id: number, title: string, block: TurboshaftGraphBlock,
              sourcePosition: SourcePosition, bytecodePosition: BytecodePosition,
              origin: NodeOrigin, opPropertiesType: OpPropertiesType) {
    super(id);
    this.title = title;
    this.block = block;
    this.sourcePosition = sourcePosition;
    this.bytecodePosition = bytecodePosition;
    this.origin = origin;
    this.opPropertiesType = opPropertiesType;
    this.visible = true;
  }

  public getHeight(showCustomData: boolean): number {
    return showCustomData ? this.labelBox.height * 2 : this.labelBox.height;
  }

  public getWidth(): number {
    return Math.max(this.inputs.length * C.NODE_INPUT_WIDTH, this.labelBox.width);
  }

  public initDisplayLabel(): void {
    this.displayLabel = this.getInlineLabel();
    this.labelBox = measureText(this.displayLabel);
  }

  public getTitle(): string {
    let title = `${this.id} ${this.title} ${this.opPropertiesType}`;
    if (this.origin) {
      title += `\nOrigin: ${this.origin.toString()}`;
    }
    if (this.inputs.length > 0) {
      title += `\nInputs: ${this.inputs.map(i => i.source.id).join(", ")}`;
    }
    if (this.outputs.length > 0) {
      title += `\nOutputs: ${this.outputs.map(i => i.target.id).join(", ")}`;
    }
    return title;
  }

  public getHistoryLabel(): string {
    return `${this.id} ${this.title}`;
  }

  public getNodeOrigin(): NodeOrigin {
    return this.origin;
  }

  public getInlineLabel(): string {
    if (this.inputs.length == 0) return `${this.id} ${this.title}`;
    return `${this.id} ${this.title}(${this.inputs.map(i => i.source.id).join(",")})`;
  }

  public equals(that?: TurboshaftGraphNode): boolean {
    if (!that) return false;
    if (this.id !== that.id) return false;
    return this.title === that.title;
  }
}

export enum OpPropertiesType {
  Pure = "Pure",
  Reading = "Reading",
  Writing = "Writing",
  CanDeopt = "CanDeopt",
  AnySideEffects = "AnySideEffects",
  BlockTerminator = "BlockTerminator"
}
