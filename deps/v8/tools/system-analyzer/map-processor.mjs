// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {transitionTypeToColor} from './helper.mjs';
import {Timeline} from './timeline.mjs';

// ===========================================================================
import {Event} from './event.mjs';
const kChunkHeight = 250;
const kChunkWidth = 10;

function define(prototype, name, fn) {
  Object.defineProperty(prototype, name, {value: fn, enumerable: false});
}

define(Array.prototype, 'max', function(fn) {
  if (this.length === 0) return undefined;
  if (fn === undefined) fn = (each) => each;
  let max = fn(this[0]);
  for (let i = 1; i < this.length; i++) {
    max = Math.max(max, fn(this[i]));
  }
  return max;
})
define(Array.prototype, 'first', function() {
  return this[0]
});
define(Array.prototype, 'last', function() {
  return this[this.length - 1]
});
// ===========================================================================

class MapProcessor extends LogReader {
  #profile = new Profile();
  #timeline = new Timeline();
  #formatPCRegexp = /(.*):[0-9]+:[0-9]+$/;
  MAJOR_VERSION = 7;
  MINOR_VERSION = 6;
  constructor() {
    super();
    this.dispatchTable_ = {
      __proto__: null,
      'code-creation': {
        parsers: [
          parseString, parseInt, parseInt, parseInt, parseInt, parseString,
          parseVarArgs
        ],
        processor: this.processCodeCreation
      },
      'v8-version': {
        parsers: [
          parseInt, parseInt,
        ],
        processor: this.processV8Version
      },
      'code-move': {
        parsers: [parseInt, parseInt],
        'sfi-move':
            {parsers: [parseInt, parseInt], processor: this.processCodeMove},
        'code-delete': {parsers: [parseInt], processor: this.processCodeDelete},
        processor: this.processFunctionMove
      },
      'map-create':
          {parsers: [parseInt, parseString], processor: this.processMapCreate},
      'map': {
        parsers: [
          parseString, parseInt, parseString, parseString, parseInt, parseInt,
          parseString, parseString, parseString
        ],
        processor: this.processMap
      },
      'map-details': {
        parsers: [parseInt, parseString, parseString],
        processor: this.processMapDetails
      }
    };
  }

  printError(str) {
    console.error(str);
    throw str
  }

  processString(string) {
    let end = string.length;
    let current = 0;
    let next = 0;
    let line;
    let i = 0;
    let entry;
    try {
      while (current < end) {
        next = string.indexOf('\n', current);
        if (next === -1) break;
        i++;
        line = string.substring(current, next);
        current = next + 1;
        this.processLogLine(line);
      }
    } catch (e) {
      console.error('Error occurred during parsing, trying to continue: ' + e);
    }
    return this.finalize();
  }

  processLogFile(fileName) {
    this.collectEntries = true;
    this.lastLogFileName_ = fileName;
    let i = 1;
    let line;
    try {
      while (line = readline()) {
        this.processLogLine(line);
        i++;
      }
    } catch (e) {
      console.error(
          'Error occurred during parsing line ' + i +
          ', trying to continue: ' + e);
    }
    return this.finalize();
  }

  finalize() {
    // TODO(cbruni): print stats;
    this.#timeline.transitions = new Map();
    let id = 0;
    this.#timeline.forEach(map => {
      if (map.isRoot()) id = map.finalizeRootMap(id + 1);
      if (map.edge && map.edge.name) {
        let edge = map.edge;
        let list = this.#timeline.transitions.get(edge.name);
        if (list === undefined) {
          this.#timeline.transitions.set(edge.name, [edge]);
        } else {
          list.push(edge);
        }
      }
    });
    return this.#timeline;
  }

  addEntry(entry) {
    this.entries.push(entry);
  }

  /**
   * Parser for dynamic code optimization state.
   */
  parseState(s) {
    switch (s) {
      case '':
        return Profile.CodeState.COMPILED;
      case '~':
        return Profile.CodeState.OPTIMIZABLE;
      case '*':
        return Profile.CodeState.OPTIMIZED;
    }
    throw new Error('unknown code state: ' + s);
  }

  processCodeCreation(type, kind, timestamp, start, size, name, maybe_func) {
    if (maybe_func.length) {
      let funcAddr = parseInt(maybe_func[0]);
      let state = this.parseState(maybe_func[1]);
      this.#profile.addFuncCode(
          type, name, timestamp, start, size, funcAddr, state);
    } else {
      this.#profile.addCode(type, name, timestamp, start, size);
    }
  }

  processV8Version(majorVersion, minorVersion){
    if(
      (majorVersion == this.MAJOR_VERSION && minorVersion <= this.MINOR_VERSION)
        || (majorVersion < this.MAJOR_VERSION)){
          window.alert(
            `Unsupported version ${majorVersion}.${minorVersion}. \n` +
              `Please use the matching tool for given the V8 version.`);
    }
  }

  processCodeMove(from, to) {
    this.#profile.moveCode(from, to);
  }

  processCodeDelete(start) {
    this.#profile.deleteCode(start);
  }

  processFunctionMove(from, to) {
    this.#profile.moveFunc(from, to);
  }

  formatPC(pc, line, column) {
    let entry = this.#profile.findEntry(pc);
    if (!entry) return '<unknown>'
      if (entry.type === 'Builtin') {
        return entry.name;
      }
    let name = entry.func.getName();
    let array = this.#formatPCRegexp.exec(name);
    if (array === null) {
      entry = name;
    } else {
      entry = entry.getState() + array[1];
    }
    return entry + ':' + line + ':' + column;
  }

  processMap(type, time, from, to, pc, line, column, reason, name) {
    let time_ = parseInt(time);
    if (type === 'Deprecate') return this.deprecateMap(type, time_, from);
    let from_ = this.getExistingMap(from, time_);
    let to_ = this.getExistingMap(to, time_);
    let edge = new Edge(type, name, reason, time, from_, to_);
    to_.filePosition = this.formatPC(pc, line, column);
    edge.finishSetup();
  }

  deprecateMap(type, time, id) {
    this.getExistingMap(id, time).deprecate();
  }

  processMapCreate(time, id) {
    // map-create events might override existing maps if the addresses get
    // recycled. Hence we do not check for existing maps.
    let map = this.createMap(id, time);
  }

  processMapDetails(time, id, string) {
    // TODO(cbruni): fix initial map logging.
    let map = this.getExistingMap(id, time);
    map.description = string;
  }

  createMap(id, time) {
    let map = new MapLogEvent(id, time);
    this.#timeline.push(map);
    return map;
  }

  getExistingMap(id, time) {
    if (id === '0x000000000000') return undefined;
    let map = MapLogEvent.get(id, time);
    if (map === undefined) {
      console.error('No map details provided: id=' + id);
      // Manually patch in a map to continue running.
      return this.createMap(id, time);
    };
    return map;
  }
}

// ===========================================================================

class MapLogEvent extends Event {
  edge = void 0;
  children = [];
  depth = 0;
  // TODO(zcankara): Change this to private class field.
  #isDeprecated = false;
  deprecatedTargets = null;
  leftId= 0;
  rightId = 0;
  filePosition = '';
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
    let xFrom = (index + 0.5) * kChunkWidth;
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
      for (let i = 0; i < maps.length; i++) {
        // TODO: Implement time based map search
        if (maps[i].time === time) {
          return maps[i];
        }
      }
      // default return the latest
      return maps[maps.length - 1];
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
    return transitionTypeToColor(this.type);
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


// ===========================================================================
class ArgumentsProcessor extends BaseArgumentsProcessor {
  getArgsDispatch() {
    return {
      '--range':
          ['range', 'auto,auto', 'Specify the range limit as [start],[end]'],
      '--source-map': [
        'sourceMap', null,
        'Specify the source map that should be used for output'
      ]
    };
  }

  getDefaultResults() {
    return {
      logFileName: 'v8.log',
      range: 'auto,auto',
    };
  }
}

export { MapProcessor, MapLogEvent, kChunkWidth, kChunkHeight};
