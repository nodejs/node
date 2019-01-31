// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class MySelection {
  selection: any;
  stringKey: (o: any) => string;

  constructor(stringKeyFnc) {
    this.selection = new Map();
    this.stringKey = stringKeyFnc;
  }

  isEmpty(): boolean {
    return this.selection.size == 0;
  }

  clear(): void {
    this.selection = new Map();
  }

  select(s: Iterable<any>, isSelected?: boolean) {
    for (const i of s) {
      if (!i) continue;
      if (isSelected == undefined) {
        isSelected = !this.selection.has(this.stringKey(i));
      }
      if (isSelected) {
        this.selection.set(this.stringKey(i), i);
      } else {
        this.selection.delete(this.stringKey(i));
      }
    }
  }

  isSelected(i: any): boolean {
    return this.selection.has(this.stringKey(i));
  }

  isKeySelected(key: string): boolean {
    return this.selection.has(key);
  }

  selectedKeys() {
    const result = new Set();
    for (const i of this.selection.keys()) {
      result.add(i);
    }
    return result;
  }

  detachSelection() {
    const result = this.selectedKeys();
    this.clear();
    return result;
  }

  [Symbol.iterator]() { return this.selection.values(); }
}
