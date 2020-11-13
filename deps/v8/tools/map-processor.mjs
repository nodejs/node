// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { LogReader, parseString, parseVarArgs } from "./logreader.mjs";
import { BaseArgumentsProcessor } from "./arguments.mjs";
import { Profile } from "./profile.mjs";

// ===========================================================================
function define(prototype, name, fn) {
  Object.defineProperty(prototype, name, {value:fn, enumerable:false});
}

define(Array.prototype, "max", function(fn) {
  if (this.length === 0) return undefined;
  if (fn === undefined) fn = (each) => each;
  let max = fn(this[0]);
  for (let i = 1; i < this.length; i++) {
    max = Math.max(max, fn(this[i]));
  }
  return max;
})
define(Array.prototype, "first", function() { return this[0] });
define(Array.prototype, "last", function() { return this[this.length - 1] });


/**
 * A thin wrapper around shell's 'read' function showing a file name on error.
 */
export function readFile(fileName) {
  try {
    return read(fileName);
  } catch (e) {
    console.log(fileName + ': ' + (e.message || e));
    throw e;
  }
}
// ===========================================================================

export class MapProcessor extends LogReader {
  constructor() {
    super();
    this.dispatchTable_ = {
      __proto__:null,
      'code-creation': {
        parsers: [parseString, parseInt, parseInt, parseInt, parseInt,
          parseString, parseVarArgs],
        processor: this.processCodeCreation
      },
      'code-move': {
        parsers: [parseInt, parseInt],
        'sfi-move': {
          parsers: [parseInt, parseInt],
          processor: this.processCodeMove
        },
        'code-delete': {
          parsers: [parseInt],
          processor: this.processCodeDelete
        },
        processor: this.processFunctionMove
      },
      'map-create': {
        parsers: [parseInt, parseString],
        processor: this.processMapCreate
      },
      'map': {
        parsers: [parseString, parseInt, parseString, parseString, parseInt, parseInt,
          parseString, parseString, parseString
        ],
        processor: this.processMap
      },
      'map-details': {
        parsers: [parseInt, parseString, parseString],
        processor: this.processMapDetails
      }
    };
    this.profile_ = new Profile();
    this.timeline_ = new Timeline();
    this.formatPCRegexp_ = /(.*):[0-9]+:[0-9]+$/;
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
        next = string.indexOf("\n", current);
        if (next === -1) break;
        i++;
        line = string.substring(current, next);
        current = next + 1;
        this.processLogLine(line);
      }
    } catch(e) {
      console.error("Error occurred during parsing, trying to continue: " + e);
    }
    return this.finalize();
  }

  processLogFile(fileName) {
    this.collectEntries = true
    this.lastLogFileName_ = fileName;
    let i = 1;
    let line;
    try {
      while (line = readline()) {
        this.processLogLine(line);
        i++;
      }
    } catch(e) {
      console.error("Error occurred during parsing line " + i + ", trying to continue: " + e);
    }
    return this.finalize();
  }

  finalize() {
    // TODO(cbruni): print stats;
    this.timeline_.finalize();
    return this.timeline_;
  }

  addEntry(entry) {
    this.entries.push(entry);
  }

  /**
   * Parser for dynamic code optimization state.
   */
  parseState(s) {
    switch (s) {
      case "":
        return Profile.CodeState.COMPILED;
      case "~":
        return Profile.CodeState.OPTIMIZABLE;
      case "*":
        return Profile.CodeState.OPTIMIZED;
    }
    throw new Error("unknown code state: " + s);
  }

  processCodeCreation(
    type, kind, timestamp, start, size, name, maybe_func) {
    if (maybe_func.length) {
      let funcAddr = parseInt(maybe_func[0]);
      let state = this.parseState(maybe_func[1]);
      this.profile_.addFuncCode(type, name, timestamp, start, size, funcAddr, state);
    } else {
      this.profile_.addCode(type, name, timestamp, start, size);
    }
  }

  processCodeMove(from, to) {
    this.profile_.moveCode(from, to);
  }

  processCodeDelete(start) {
    this.profile_.deleteCode(start);
  }

  processFunctionMove(from, to) {
    this.profile_.moveFunc(from, to);
  }

  formatPC(pc, line, column) {
    let entry = this.profile_.findEntry(pc);
    if (!entry) return "<unknown>"
    if (entry.type === "Builtin") {
      return entry.name;
    }
    let name = entry.func.getName();
    let array = this.formatPCRegexp_.exec(name);
    if (array === null) {
      entry = name;
    } else {
      entry = entry.getState() + array[1];
    }
    return entry + ":" + line + ":" + column;
  }

  processMap(type, time, from, to, pc, line, column, reason, name) {
    let time_ = parseInt(time);
    if (type === "Deprecate") return this.deprecateMap(type, time_, from);
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
    //TODO(cbruni): fix initial map logging.
    let map = this.getExistingMap(id, time);
    map.description = string;
  }

  createMap(id, time) {
    let map = new V8Map(id, time);
    this.timeline_.push(map);
    return map;
  }

  getExistingMap(id, time) {
    if (id === "0x000000000000") return undefined;
    let map = V8Map.get(id, time);
    if (map === undefined) {
      console.error("No map details provided: id=" + id);
      // Manually patch in a map to continue running.
      return this.createMap(id, time);
    };
    return map;
  }
}

// ===========================================================================

class V8Map {
  constructor(id, time = -1) {
    if (!id) throw "Invalid ID";
    this.id = id;
    this.time = time;
    if (!(time > 0)) throw "Invalid time";
    this.description = "";
    this.edge = void 0;
    this.children = [];
    this.depth = 0;
    this._isDeprecated = false;
    this.deprecationTargets = null;
    V8Map.set(id, this);
    this.leftId = 0;
    this.rightId = 0;
    this.filePosition = "";
  }

  finalizeRootMap(id) {
    let stack = [this];
    while (stack.length > 0) {
      let current = stack.pop();
      if (current.leftId !== 0) {
        console.error("Skipping potential parent loop between maps:", current)
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
    if (id <= 0) throw "invalid id";
    let currentId = id;
    this.leftId = currentId
  }


  parent() {
    if (this.edge === void 0) return void 0;
    return this.edge.from;
  }

  isDeprecated() {
    return this._isDeprecated;
  }

  deprecate() {
    this._isDeprecated = true;
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

  getType() {
    return this.edge === void 0 ? "new" : this.edge.type;
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
    if(maps){
      for (let i = 0; i < maps.length; i++) {
        //TODO: Implement time based map search
        if(maps[i].time === time){
          return maps[i];
        }
      }
      // default return the latest
      return maps[maps.length-1];
    }
  }

  static set(id, map) {
    if(this.cache.has(id)){
      this.cache.get(id).push(map);
    } else {
      this.cache.set(id, [map]);
    }
  }
}

V8Map.cache = new Map();



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

  finishSetup() {
    let from = this.from
    if (from) from.addEdge(this);
    let to = this.to;
    if (to === undefined) return;
    to.edge = this;
    if (from === undefined ) return;
    if (to === from) throw "From and to must be distinct.";
    if (to.time < from.time) {
      console.error("invalid time order");
    }
    let newDepth = from.depth + 1;
    if (to.depth > 0 && to.depth != newDepth) {
      console.error("Depth has already been initialized");
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
    return this.type === "Transition"
  }

  isFastToSlow() {
    return this.type === "Normalize"
  }

  isSlowToFast() {
    return this.type === "SlowToFast"
  }

  isInitial() {
    return this.type === "InitialMap"
  }

  isBootstrapped() {
    return this.type === "new"
  }

  isReplaceDescriptors() {
    return this.type === "ReplaceDescriptors"
  }

  isCopyAsPrototype() {
    return this.reason === "CopyAsPrototype"
  }

  isOptimizeAsPrototype() {
    return this.reason === "OptimizeAsPrototype"
  }

  symbol() {
    if (this.isTransition()) return "+";
    if (this.isFastToSlow()) return "⊡";
    if (this.isSlowToFast()) return "⊛";
    if (this.isReplaceDescriptors()) {
      if (this.name) return "+";
      return "∥";
    }
    return "";
  }

  toString() {
    let s = this.symbol();
    if (this.isTransition()) return s + this.name;
    if (this.isFastToSlow()) return s + this.reason;
    if (this.isCopyAsPrototype()) return s + "Copy as Prototype";
    if (this.isOptimizeAsPrototype()) {
      return s + "Optimize as Prototype";
    }
    if (this.isReplaceDescriptors() && this.name) {
      return this.type + " " + this.symbol() + this.name;
    }
    return this.type + " " + (this.reason ? this.reason : "") + " " +
      (this.name ? this.name : "")
  }
}


// ===========================================================================
class Marker {
  constructor(time, name) {
    this.time = parseInt(time);
    this.name = name;
  }
}

// ===========================================================================
class Timeline {
  constructor() {
    this.values = [];
    this.transitions = new Map();
    this.markers = [];
    this.startTime = 0;
    this.endTime = 0;
  }

  push(map) {
    let time = map.time;
    if (!this.isEmpty() && this.last().time > time) {
      // Invalid insertion order, might happen without --single-process,
      // finding insertion point.
      let insertionPoint = this.find(time);
      this.values.splice(insertionPoint, map);
    } else {
      this.values.push(map);
    }
    if (time > 0) {
      this.endTime = Math.max(this.endTime, time);
      if (this.startTime === 0) {
        this.startTime = time;
      } else {
        this.startTime = Math.min(this.startTime, time);
      }
    }
  }

  addMarker(time, message) {
    this.markers.push(new Marker(time, message));
  }

  finalize() {
    let id = 0;
    this.forEach(map => {
        if (map.isRoot()) id = map.finalizeRootMap(id + 1);
        if (map.edge && map.edge.name) {
          let edge = map.edge;
          let list = this.transitions.get(edge.name);
          if (list === undefined) {
            this.transitions.set(edge.name, [edge]);
          } else {
            list.push(edge);
          }
        }
    });
    this.markers.sort((a, b) => b.time - a.time);
  }

  at(index) {
    return this.values[index]
  }

  isEmpty() {
    return this.size() === 0
  }

  size() {
    return this.values.length
  }

  first() {
    return this.values.first()
  }

  last() {
    return this.values.last()
  }

  duration() {
    return this.last().time - this.first().time
  }

  forEachChunkSize(count, fn) {
    const increment = this.duration() / count;
    let currentTime = this.first().time + increment;
    let index = 0;
    for (let i = 0; i < count; i++) {
      let nextIndex = this.find(currentTime, index);
      let nextTime = currentTime + increment;
      fn(index, nextIndex, currentTime, nextTime);
      index = nextIndex
      currentTime = nextTime;
    }
  }

  chunkSizes(count) {
    let chunks = [];
    this.forEachChunkSize(count, (start, end) => chunks.push(end - start));
    return chunks;
  }

  chunks(count) {
    let chunks = [];
    let emptyMarkers = [];
    this.forEachChunkSize(count, (start, end, startTime, endTime) => {
      let items = this.values.slice(start, end);
      let markers = this.markersAt(startTime, endTime);
      chunks.push(new Chunk(chunks.length, startTime, endTime, items, markers));
    });
    return chunks;
  }

  range(start, end) {
    const first = this.find(start);
    if (first < 0) return [];
    const last = this.find(end, first);
    return this.values.slice(first, last);
  }

  find(time, offset = 0) {
    return this.basicFind(this.values, each => each.time - time, offset);
  }

  markersAt(startTime, endTime) {
    let start = this.basicFind(this.markers, each => each.time - startTime);
    let end = this.basicFind(this.markers, each => each.time - endTime, start);
    return this.markers.slice(start, end);
  }

  basicFind(array, cmp, offset = 0) {
    let min = offset;
    let max = array.length;
    while (min < max) {
      let mid = min + Math.floor((max - min) / 2);
      let result = cmp(array[mid]);
      if (result > 0) {
        max = mid - 1;
      } else {
        min = mid + 1;
      }
    }
    return min;
  }

  count(filter) {
    return this.values.reduce((sum, each) => {
      return sum + (filter(each) === true ? 1 : 0);
    }, 0);
  }

  filter(predicate) {
    return this.values.filter(predicate);
  }

  filterUniqueTransitions(filter) {
    // Returns a list of Maps whose parent is not in the list.
    return this.values.filter(map => {
      if (filter(map) === false) return false;
      let parent = map.parent();
      if (parent === undefined) return true;
      return filter(parent) === false;
    });
  }

  depthHistogram() {
    return this.values.histogram(each => each.depth);
  }

  fanOutHistogram() {
    return this.values.histogram(each => each.children.length);
  }

  forEach(fn) {
    return this.values.forEach(fn)
  }
}


// ===========================================================================
class Chunk {
  constructor(index, start, end, items, markers) {
    this.index = index;
    this.start = start;
    this.end = end;
    this.items = items;
    this.markers = markers
    this.height = 0;
  }

  isEmpty() {
    return this.items.length === 0;
  }

  last() {
    return this.at(this.size() - 1);
  }

  first() {
    return this.at(0);
  }

  at(index) {
    return this.items[index];
  }

  size() {
    return this.items.length;
  }

  yOffset(map) {
    // items[0]   == oldest map, displayed at the top of the chunk
    // items[n-1] == youngest map, displayed at the bottom of the chunk
    return (1 - (this.indexOf(map) + 0.5) / this.size()) * this.height;
  }

  indexOf(map) {
    return this.items.indexOf(map);
  }

  has(map) {
    if (this.isEmpty()) return false;
    return this.first().time <= map.time && map.time <= this.last().time;
  }

  next(chunks) {
    return this.findChunk(chunks, 1);
  }

  prev(chunks) {
    return this.findChunk(chunks, -1);
  }

  findChunk(chunks, delta) {
    let i = this.index + delta;
    let chunk = chunks[i];
    while (chunk && chunk.size() === 0) {
      i += delta;
      chunk = chunks[i]
    }
    return chunk;
  }

  getTransitionBreakdown() {
    return BreakDown(this.items, map => map.getType())
  }

  getUniqueTransitions() {
    // Filter out all the maps that have parents within the same chunk.
    return this.items.filter(map => !map.parent() || !this.has(map.parent()));
  }
}


// ===========================================================================
function BreakDown(list, map_fn) {
  if (map_fn === void 0) {
    map_fn = each => each;
  }
  let breakdown = {__proto__:null};
  list.forEach(each=> {
    let type = map_fn(each);
    let v = breakdown[type];
    breakdown[type] = (v | 0) + 1
  });
  return Object.entries(breakdown)
    .sort((a,b) => a[1] - b[1]);
}


// ===========================================================================
export class ArgumentsProcessor extends BaseArgumentsProcessor {
  getArgsDispatch() {
    return {
      '--range': ['range', 'auto,auto',
        'Specify the range limit as [start],[end]'
      ],
      '--source-map': ['sourceMap', null,
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
