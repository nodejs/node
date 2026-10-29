// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {groupBy} from './helper.mjs'

/** @template T */
class Timeline {
  /** Class T */
  _model;
  /** @type {T[]} */
  _values;
  // Current selection, subset of #values:
  /** @type {?Timeline<T>} */
  _selection;
  _breakdown;

  constructor(model, values = [], startTime = null, endTime = null) {
    this._model = model;
    this._values = values;
    /** @type {number} */
    this.startTime = startTime;
    /** @type {number} */
    this.endTime = endTime;
    if (values.length > 0) {
      if (startTime === null) this.startTime = values[0].time;
      if (endTime === null) this.endTime = values[values.length - 1].time;
    } else {
      if (startTime === null) this.startTime = 0;
      if (endTime === null) this.endTime = 0;
    }
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

  /** @returns {Timeline<T>} */
  get selectionOrSelf() {
    return this._selection ?? this;
  }

  /** @param {Timeline<T>} value */
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

  /** @returns {T[]} */
  get values() {
    return this._values;
  }

  /** @returns {number} */
  count(filter) {
    return this.all.reduce((sum, each) => {
      return sum + (filter(each) === true ? 1 : 0);
    }, 0);
  }

  filter(predicate) {
    return this.all.filter(predicate);
  }

  /** @param {T} event */
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

  /** @returns {T} */
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

  /** @returns {T[]} */
  slice(startIndex, endIndex) {
    return this._values.slice(startIndex, endIndex);
  }

  /** @returns {T} */
  first() {
    return this._values[0];
  }

  /** @returns {T} */
  last() {
    return this._values[this._values.length - 1];
  }

  * [Symbol.iterator]() {
    yield* this._values;
  }

  /** @returns {number} */
  duration() {
    return this.endTime - this.startTime;
  }

  forEachChunkSize(count, fn) {
    if (this.isEmpty()) return;
    const increment = this.duration() / count;
    let currentTime = this.startTime;
    let index = 0;
    for (let i = 0; i < count - 1; i++) {
      const nextTime = currentTime + increment;
      const nextIndex = this.findLast(nextTime, index);
      fn(index, nextIndex, currentTime, nextTime);
      index = nextIndex + 1;
      currentTime = nextTime;
    }
    fn(index, this._values.length - 1, currentTime, this.endTime);
  }

  /** @returns {number[]} */
  chunkSizes(count) {
    const chunks = [];
    this.forEachChunkSize(count, (start, end) => chunks.push(end - start));
    return chunks;
  }

  /** @returns {Chunk<T>[]} */
  chunks(count, predicate = undefined) {
    const chunks = [];
    this.forEachChunkSize(count, (start, end, startTime, endTime) => {
      let items = this._values.slice(start, end + 1);
      if (predicate !== undefined) items = items.filter(predicate);
      chunks.push(new Chunk(chunks.length, startTime, endTime, items));
    });
    return chunks;
  }

  /**
   * Return all entries in ({startTime}, {endTime}]
   * @returns {T[]}
   **/
  range(startTime, endTime) {
    const firstIndex = this.find(startTime);
    if (firstIndex < 0) return [];
    const lastIndex = this.find(endTime, firstIndex + 1);
    return this._values.slice(firstIndex, lastIndex);
  }

  // Return the first index with element.time >= time.
  find(time, offset = 0) {
    return this.findFirst(time, offset);
  }

  findFirst(time, offset = 0) {
    return this._find(this._values, each => each.time - time, offset);
  }

  // Return the last index with element.time <= time.
  findLast(time, offset = 0) {
    const nextTime = time + 1;
    let index = (this.last().time <= nextTime) ?
        this.length :
        this.findFirst(nextTime + 1, offset);
    // Typically we just have to look at the previous element.
    while (index > 0) {
      index--;
      if (this._values[index].time <= time) return index;
    }
    return -1;
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

  getBreakdown(keyFunction, collect = false) {
    if (keyFunction || collect) {
      if (!keyFunction) {
        keyFunction = each => each.type;
      }
      return groupBy(this._values, keyFunction, collect);
    }
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
/** @template T */
class Chunk {
  constructor(index, start, end, items) {
    this.index = index;
    this.start = start;
    this.end = end;
    /** @type {T[]} */
    this.items = items;
    this.height = 0;
  }

  isEmpty() {
    return this.items.length === 0;
  }

  /** @returns {T} */
  last() {
    return this.at(this.size() - 1);
  }

  /** @returns {T} */
  first() {
    return this.at(0);
  }

  /** @returns {T} */
  at(index) {
    return this.items[index];
  }

  size() {
    return this.items.length;
  }

  get length() {
    return this.items.length;
  }

  /** @param {T} event */
  yOffset(event) {
    // items[0]   == oldest event, displayed at the top of the chunk
    // items[n-1] == youngest event, displayed at the bottom of the chunk
    return ((this.indexOf(event) + 0.5) / this.size()) * this.height;
  }

  /** @param {T} event */
  indexOf(event) {
    return this.items.indexOf(event);
  }

  /** @param {T} event */
  has(event) {
    if (this.isEmpty()) return false;
    return this.first().time <= event.time && event.time <= this.last().time;
  }

  /** @param {Chunk<T>[]} chunks */
  next(chunks) {
    return this.findChunk(chunks, 1);
  }

  /** @param {Chunk<T>[]} chunks */
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

  /** @returns {T[]} */
  filter() {
    return this.items.filter(map => !map.parent || !this.has(map.parent));
  }
}

export {Timeline, Chunk};
