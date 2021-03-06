// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {groupBy} from './helper.mjs'

class Timeline {
  // Class:
  _model;
  // Array of #model instances:
  _values;
  // Current selection, subset of #values:
  _selection;
  _breakdown;

  constructor(model, values = [], startTime = 0, endTime = 0) {
    this._model = model;
    this._values = values;
    this.startTime = startTime;
    this.endTime = endTime;
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

  get selectionOrSelf() {
    return this._selection ?? this;
  }

  set selection(value) {
    this._selection = value;
  }

  selectTimeRange(startTime, endTime) {
    const items = this.range(startTime, endTime);
    this._selection = new Timeline(this._model, items, startTime, endTime);
  }

  clearSelection() {
    this._selection = undefined;
  }

  getChunks(windowSizeMs) {
    return this.chunkSizes(windowSizeMs);
  }

  get values() {
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

  slice(startIndex, endIndex) {
    return this._values.slice(startIndex, endIndex);
  }

  first() {
    return this._values[0];
  }

  last() {
    return this._values[this._values.length - 1];
  }

  * [Symbol.iterator]() {
    yield* this._values;
  }

  duration() {
    if (this.isEmpty()) return 0;
    return this.last().time - this.first().time;
  }

  forEachChunkSize(count, fn) {
    if (this.isEmpty()) return;
    const increment = this.duration() / count;
    let currentTime = this.first().time + increment;
    let index = 0;
    for (let i = 0; i < count; i++) {
      const nextIndex = this.find(currentTime, index);
      const nextTime = currentTime + increment;
      fn(index, nextIndex, currentTime, nextTime);
      index = nextIndex;
      currentTime = nextTime;
    }
  }

  chunkSizes(count) {
    const chunks = [];
    this.forEachChunkSize(count, (start, end) => chunks.push(end - start));
    return chunks;
  }

  chunks(count, predicate = undefined) {
    const chunks = [];
    this.forEachChunkSize(count, (start, end, startTime, endTime) => {
      let items = this._values.slice(start, end);
      if (predicate !== undefined) items = items.filter(predicate);
      chunks.push(new Chunk(chunks.length, startTime, endTime, items));
    });
    return chunks;
  }

  // Return all entries in ({startTime}, {endTime}]
  range(startTime, endTime) {
    const firstIndex = this.find(startTime);
    if (firstIndex < 0) return [];
    const lastIndex = this.find(endTime, firstIndex + 1);
    return this._values.slice(firstIndex, lastIndex);
  }

  // Return the first index for the first element at {time}.
  find(time, offset = 0) {
    return this._find(this._values, each => each.time - time, offset);
  }

  // Return the first index for which compareFn(item) is >= 0;
  _find(array, compareFn, offset = 0) {
    let minIndex = offset;
    let maxIndex = array.length - 1;
    while (minIndex < maxIndex) {
      const midIndex = minIndex + (((maxIndex - minIndex) / 2) | 0);
      if (compareFn(array[midIndex]) < 0) {
        minIndex = midIndex + 1;
      } else {
        maxIndex = midIndex;
      }
    }
    return minIndex;
  }

  getBreakdown(keyFunction) {
    if (keyFunction) return groupBy(this._values, keyFunction);
    if (this._breakdown === undefined) {
      this._breakdown = groupBy(this._values, each => each.type);
    }
    return this._breakdown;
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

  get length() {
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

  getBreakdown(keyFunction) {
    return groupBy(this.items, keyFunction);
  }

  filter() {
    return this.items.filter(map => !map.parent() || !this.has(map.parent()));
  }
}

export {Timeline, Chunk};
