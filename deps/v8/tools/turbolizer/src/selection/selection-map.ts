// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { GraphNode } from "../phases/graph-phase/graph-node";

export class SelectionMap {
  selection: Map<string, any>;
  stringKey: (obj: any) => string;
  originStringKey: (node: GraphNode) => string;

  constructor(stringKeyFnc, originStringKeyFnc?) {
    this.selection = new Map<string, any>();
    this.stringKey = stringKeyFnc;
    this.originStringKey = originStringKeyFnc;
  }

  public isEmpty(): boolean {
    return this.selection.size == 0;
  }

  public clear(): void {
    this.selection = new Map<string, any>();
  }

  public select(items: Iterable<any>, isSelected?: boolean): void {
    for (const item of items) {
      if (item === undefined) continue;
      if (isSelected === undefined) {
        isSelected = !this.selection.has(this.stringKey(item));
      }
      if (isSelected) {
        this.selection.set(this.stringKey(item), item);
      } else {
        this.selection.delete(this.stringKey(item));
      }
    }
  }

  public isSelected(obj: any): boolean {
    return this.selection.has(this.stringKey(obj));
  }

  public isKeySelected(key: string): boolean {
    return this.selection.has(key);
  }

  public selectedKeys(): Set<string> {
    const result = new Set<string>();
    for (const key of this.selection.keys()) {
      result.add(key);
    }
    return result;
  }

  public selectedKeysAsAbsNumbers(): Set<number> {
    const result = new Set<number>();
    for (const key of this.selection.keys()) {
      const keyNum = Number(key);
      result.add(keyNum < 0 ? Math.abs(keyNum + 1) : keyNum);
    }
    return result;
  }

  public detachSelection(): Map<string, any> {
    const result = this.selection;
    this.clear();
    return result;
  }

  [Symbol.iterator]() { return this.selection.values(); }
}