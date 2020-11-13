// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { typeToColor } from '../helper.mjs';
import { Event } from './log.mjs';

// ===========================================================================
// Map Log Events

const kChunkHeight = 250;
const kChunkWidth = 10;

function define(prototype, name, fn) {
  Object.defineProperty(prototype, name, { value: fn, enumerable: false });
}

define(Array.prototype, 'max', function (fn) {
  if (this.length === 0) return undefined;
  if (fn === undefined) fn = (each) => each;
  let max = fn(this[0]);
  for (let i = 1; i < this.length; i++) {
    max = Math.max(max, fn(this[i]));
  }
  return max;
})
define(Array.prototype, 'first', function () {
  return this[0]
});
define(Array.prototype, 'last', function () {
  return this[this.length - 1]
});

// ===========================================================================
// Map Log Events

class MapLogEvent extends Event {
  edge = void 0;
  children = [];
  depth = 0;
  // TODO(zcankara): Change this to private class field.
  #isDeprecated = false;
  deprecatedTargets = null;
  leftId = 0;
  rightId = 0;
  filePosition = '';
  script = '';
  id = -1;
  constructor(id, time) {
    if (!time) throw new Error('Invalid time');
    super(id, time);
    MapLogEvent.set(id, this);
    this.id = id;
  }

  finalizeRootMap(id) {
    let stack = [this];
    while (stack.length > 0) {
      let current = stack.pop();
      if (current.leftId !== 0) {
        console.error('Skipping potential parent loop between maps:', current)
        continue;
      }
      current.finalize(id)
      id += 1;
      current.children.forEach(edge => stack.push(edge.to))
      // TODO implement rightId
    }
    return id;
  }

  finalize(id) {
    // Initialize preorder tree traversal Ids for fast subtree inclusion checks
    if (id <= 0) throw 'invalid id';
    let currentId = id;
    this.leftId = currentId
  }

  parent() {
    if (this.edge === void 0) return void 0;
    return this.edge.from;
  }

  isDeprecated() {
    return this.#isDeprecated;
  }

  deprecate() {
    this.#isDeprecated = true;
  }

  isRoot() {
    return this.edge === void 0 || this.edge.from === void 0;
  }

  contains(map) {
    return this.leftId < map.leftId && map.rightId < this.rightId;
  }

  addEdge(edge) {
    this.children.push(edge);
  }

  chunkIndex(chunks) {
    // Did anybody say O(n)?
    for (let i = 0; i < chunks.length; i++) {
      let chunk = chunks[i];
      if (chunk.isEmpty()) continue;
      if (chunk.last().time < this.time) continue;
      return i;
    }
    return -1;
  }

  position(chunks) {
    let index = this.chunkIndex(chunks);
    let xFrom = (index + 1.5) * kChunkWidth;
    let yFrom = kChunkHeight - chunks[index].yOffset(this);
    return [xFrom, yFrom];
  }

  transitions() {
    let transitions = Object.create(null);
    let current = this;
    while (current) {
      let edge = current.edge;
      if (edge && edge.isTransition()) {
        transitions[edge.name] = edge;
      }
      current = current.parent()
    }
    return transitions;
  }

  get type() {
    return this.edge === void 0 ? 'new' : this.edge.type;
  }

  isBootstrapped() {
    return this.edge === void 0;
  }

  getParents() {
    let parents = [];
    let current = this.parent();
    while (current) {
      parents.push(current);
      current = current.parent();
    }
    return parents;
  }

  static get(id, time = undefined) {
    let maps = this.cache.get(id);
    if (maps) {
      for (let i = 1; i < maps.length; i++) {
        if (maps[i].time > time) {
          return maps[i - 1];
        }
      }
      // default return the latest
      return (maps.length > 0) ? maps[maps.length - 1] : undefined;
    }
  }

  static set(id, map) {
    if (this.cache.has(id)) {
      this.cache.get(id).push(map);
    } else {
      this.cache.set(id, [map]);
    }
  }
}

MapLogEvent.cache = new Map();

// ===========================================================================
class Edge {
  constructor(type, name, reason, time, from, to) {
    this.type = type;
    this.name = name;
    this.reason = reason;
    this.time = time;
    this.from = from;
    this.to = to;
  }

  getColor() {
    return typeToColor(this.type);
  }

  finishSetup() {
    let from = this.from;
    if (from) from.addEdge(this);
    let to = this.to;
    if (to === undefined) return;
    to.edge = this;
    if (from === undefined) return;
    if (to === from) throw 'From and to must be distinct.';
    if (to.time < from.time) {
      console.error('invalid time order');
    }
    let newDepth = from.depth + 1;
    if (to.depth > 0 && to.depth != newDepth) {
      console.error('Depth has already been initialized');
    }
    to.depth = newDepth;
  }

  chunkIndex(chunks) {
    // Did anybody say O(n)?
    for (let i = 0; i < chunks.length; i++) {
      let chunk = chunks[i];
      if (chunk.isEmpty()) continue;
      if (chunk.last().time < this.time) continue;
      return i;
    }
    return -1;
  }

  parentEdge() {
    if (!this.from) return undefined;
    return this.from.edge;
  }

  chainLength() {
    let length = 0;
    let prev = this;
    while (prev) {
      prev = this.parent;
      length++;
    }
    return length;
  }

  isTransition() {
    return this.type === 'Transition'
  }

  isFastToSlow() {
    return this.type === 'Normalize'
  }

  isSlowToFast() {
    return this.type === 'SlowToFast'
  }

  isInitial() {
    return this.type === 'InitialMap'
  }

  isBootstrapped() {
    return this.type === 'new'
  }

  isReplaceDescriptors() {
    return this.type === 'ReplaceDescriptors'
  }

  isCopyAsPrototype() {
    return this.reason === 'CopyAsPrototype'
  }

  isOptimizeAsPrototype() {
    return this.reason === 'OptimizeAsPrototype'
  }

  symbol() {
    if (this.isTransition()) return '+';
    if (this.isFastToSlow()) return '⊡';
    if (this.isSlowToFast()) return '⊛';
    if (this.isReplaceDescriptors()) {
      if (this.name) return '+';
      return '∥';
    }
    return '';
  }

  toString() {
    let s = this.symbol();
    if (this.isTransition()) return s + this.name;
    if (this.isFastToSlow()) return s + this.reason;
    if (this.isCopyAsPrototype()) return s + 'Copy as Prototype';
    if (this.isOptimizeAsPrototype()) {
      return s + 'Optimize as Prototype';
    }
    if (this.isReplaceDescriptors() && this.name) {
      return this.type + ' ' + this.symbol() + this.name;
    }
    return this.type + ' ' + (this.reason ? this.reason : '') + ' ' +
      (this.name ? this.name : '')
  }
}


export { MapLogEvent, Edge, kChunkWidth, kChunkHeight };
