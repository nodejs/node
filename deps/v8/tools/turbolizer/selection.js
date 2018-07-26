// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class Selection {
  constructor(stringKeyFnc) {
    this.selection = new Map();
    this.stringKey = stringKeyFnc;
  }

  isEmpty() {
    return this.selection.size == 0;
  }

  clear() {
    this.selection = new Map();
  }

  select(s, isSelected) {
    if (!isIterable(s)) { s = [s]; }
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

  isSelected(i) {
    return this.selection.has(this.stringKey(i));
  }

  isKeySelected(key) {
    return this.selection.has(key);
  }

  selectedKeys() {
    var result = new Set();
    for (var i of this.selection.keys()) {
      result.add(i);
    }
    return result;
  }

  detachSelection() {
    var result = new Set();
    for (var i of this.selection.keys()) {
      result.add(i);
    }
    this.clear();
    return result;
  }

  [Symbol.iterator]() { return this.selection.values() }
}
