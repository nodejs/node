// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { TurboshaftGraphNode } from "./turboshaft-graph-node";
import { Edge } from "../../edge";
import { TurboshaftGraphBlock } from "./turboshaft-graph-block";

export class TurboshaftGraphEdge<Type extends TurboshaftGraphNode | TurboshaftGraphBlock> extends
  Edge<Type> {

  constructor(target: Type, source: Type) {
    super(target, source);
    this.visible = target.visible && source.visible;
  }

  public toString(idx?: number): string {
    if (idx !== null) return `${this.source.id},${idx},${this.target.id}`;
    return `${this.source.id},${this.target.id}`;
  }
}
