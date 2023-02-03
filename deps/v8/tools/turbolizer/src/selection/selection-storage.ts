// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class SelectionStorage {
  nodes: Map<string, any>;
  blocks: Map<string, any>;
  adaptedNodes: Set<string>;
  adaptedBocks: Set<string>;

  constructor(nodes?: Map<string, any>, blocks?: Map<string, any>) {
    this.nodes = nodes ?? new Map<string, any>();
    this.blocks = blocks ?? new Map<string, any>();
    this.adaptedNodes = new Set<string>();
    this.adaptedBocks = new Set<string>();
  }

  public adaptNode(nodeKey: string): void {
    this.adaptedNodes.add(nodeKey);
  }

  public adaptBlock(blockKey: string): void {
    this.adaptedBocks.add(blockKey);
  }

  public isAdapted(): boolean {
    return this.adaptedNodes.size != 0 || this.adaptedBocks.size != 0;
  }
}
