// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as C from "../../common/constants";
import { measureText } from "../../common/util";
import { TurboshaftGraphEdge } from "./turboshaft-graph-edge";
import { TurboshaftGraphBlock } from "./turboshaft-graph-block";
import { Node } from "../../node";

export class TurboshaftGraphNode extends Node<TurboshaftGraphEdge<TurboshaftGraphNode>> {
  title: string;
  block: TurboshaftGraphBlock;
  opPropertiesType: OpPropertiesType;
  properties: string;

  constructor(id: number, title: string, block: TurboshaftGraphBlock,
              opPropertiesType: OpPropertiesType, properties: string) {
    super(id, `${id} ${title}`);
    this.title = title;
    this.block = block;
    this.opPropertiesType = opPropertiesType;
    this.properties = properties;
    this.visible = true;
  }

  public getHeight(showProperties: boolean): number {
    if (this.properties && showProperties) {
      return this.labelBox.height * 2;
    }
    return this.labelBox.height;
  }

  public getWidth(): number {
    const measure = measureText(
      `${this.getInlineLabel()}[${this.getPropertiesTypeAbbreviation()}]`
    );
    return Math.max(this.inputs.length * C.NODE_INPUT_WIDTH, measure.width);
  }

  public getInlineLabel(): string {
    if (this.inputs.length == 0) return `${this.id} ${this.title}`;
    return `${this.id} ${this.title}(${this.inputs.map(i => i.source.id).join(",")})`;
  }

  public getReadableProperties(blockWidth: number): string {
    const propertiesWidth = measureText(this.properties).width;
    if (blockWidth > propertiesWidth) return this.properties;
    const widthOfOneSymbol = Math.floor(propertiesWidth / this.properties.length);
    const lengthOfReadableProperties = Math.floor(blockWidth / widthOfOneSymbol);
    return `${this.properties.slice(0, lengthOfReadableProperties - 3)}..`;
  }

  public getPropertiesTypeAbbreviation(): string {
    switch (this.opPropertiesType) {
      case OpPropertiesType.Pure:
        return "P";
      case OpPropertiesType.Reading:
        return "R";
      case OpPropertiesType.Writing:
        return "W";
      case OpPropertiesType.CanDeopt:
        return "CD";
      case OpPropertiesType.AnySideEffects:
        return "ASE";
      case OpPropertiesType.BlockTerminator:
        return "BT";
    }
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
