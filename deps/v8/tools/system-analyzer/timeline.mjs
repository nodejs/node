// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class Timeline {
  // Class:
  _model;
  // Array of #model instances:
  _values;
  // Current selection, subset of #values:
  _selection;
  _uniqueTypes;

  constructor(model) {
    this._model = model;
    this._values = [];
    this.startTime = 0;
    this.endTime = 0;
  }

  get model() {
    return this._model;
  }

  get all() {
    return this._values;
  }

  get selection() {
    return this._selection;
  }

  set selection(value) {
    this._selection = value;
  }

  selectTimeRange(start, end) {
    this._selection = this.filter(e => e.time >= start && e.time <= end);
  }

  getChunks(windowSizeMs) {
    // TODO(zcankara) Fill this one
    return this.chunkSizes(windowSizeMs);
  }

  get values() {
    // TODO(zcankara) Not to break something delete later
    return this._values;
  }

  count(filter) {
    return this.all.reduce((sum, each) => {
      return sum + (filter(each) === true ? 1 : 0);
    }, 0);
  }

  filter(predicate) {
    return this.all.filter(predicate);
  }

  push(event) {
    let time = event.time;
    if (!this.isEmpty() && this.last().time > time) {
      // Invalid insertion order, might happen without --single-process,
      // finding insertion point.
      let insertionPoint = this.find(time);
      this._values.splice(insertionPoint, event);
    } else {
      this._values.push(event);
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

  at(index) {
    return this._values[index];
  }

  isEmpty() {
    return this.size() === 0;
  }

  size() {
    return this._values.length;
  }

  get length() {
    return this._values.length;
  }

  first() {
    return this._values[0];
  }

  last() {
    return this._values[this._values.length - 1];
  }

  duration() {
    return this.last().time - this.first().time;
  }

  forEachChunkSize(count, fn) {
    const increment = this.duration() / count;
    let currentTime = this.first().time + increment;
    let index = 0;
    for (let i = 0; i < count; i++) {
      let nextIndex = this.find(currentTime, index);
      let nextTime = currentTime + increment;
      fn(index, nextIndex, currentTime, nextTime);
      index = nextIndex;
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
    this.forEachChunkSize(count, (start, end, startTime, endTime) => {
      let items = this._values.slice(start, end);
      chunks.push(new Chunk(chunks.length, startTime, endTime, items));
    });
    return chunks;
  }

  range(start, end) {
    const first = this.find(start);
    if (first < 0) return [];
    const last = this.find(end, first);
    return this._values.slice(first, last);
  }

  find(time, offset = 0) {
    return this._find(this._values, each => each.time - time, offset);
  }

  _find(array, cmp, offset = 0) {
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

  initializeTypes() {
    const types = new Map();
    for (const entry of this.all) {
      types.get(entry.type)?.push(entry) ?? types.set(entry.type, [entry])
    }
    return this._uniqueTypes = types;
  }

  get uniqueTypes() {
    return this._uniqueTypes ?? this.initializeTypes();
  }

  depthHistogram() {
    return this._values.histogram(each => each.depth);
  }

  fanOutHistogram() {
    return this._values.histogram(each => each.children.length);
  }

  forEach(fn) {
    return this._values.forEach(fn);
  }
}

// ===========================================================================
class Chunk {
  constructor(index, start, end, items) {
    this.index = index;
    this.start = start;
    this.end = end;
    this.items = items;
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

  yOffset(event) {
    // items[0]   == oldest event, displayed at the top of the chunk
    // items[n-1] == youngest event, displayed at the bottom of the chunk
    return (1 - (this.indexOf(event) + 0.5) / this.size()) * this.height;
  }

  indexOf(event) {
    return this.items.indexOf(event);
  }

  has(event) {
    if (this.isEmpty()) return false;
    return this.first().time <= event.time && event.time <= this.last().time;
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
      chunk = chunks[i];
    }
    return chunk;
  }

  getBreakdown(event_fn) {
    if (event_fn === void 0) {
      event_fn = each => each;
    }
    let breakdown = {__proto__: null};
    this.items.forEach(each => {
      const type = event_fn(each);
      const v = breakdown[type];
      breakdown[type] = (v | 0) + 1;
    });
    return Object.entries(breakdown).sort((a, b) => a[1] - b[1]);
  }

  filter() {
    return this.items.filter(map => !map.parent() || !this.has(map.parent()));
  }
}

export {Timeline, Chunk};
