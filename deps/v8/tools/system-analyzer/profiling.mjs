// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TickLogEntry} from './log/tick.mjs';

const kForward = 1;
const kBackwards = -1;

/**
 * The StackSorter sorts ticks by recursively grouping the most frequent frames
 * at each stack-depth to the beginning. This is used to generate flame graphs.
 *
 * Example
 *  tick1 = [a1, b1]
 *  tick2 = [a0, b0]
 *  tick3 = [a1, b1, c1]
 *  tick4 = [a1, b0, c0, d0]
 *
 * If we sort this recursively from the beginning we get:
 *  tick1 = [a1, b1]
 *  tick3 = [a1, b1, c1]
 *  tick4 = [a1, b0, c0, d0]
 *  tick2 = [a0, b0]
 *
 * The idea is to group common stacks together to generate easily readable
 * graphs where we can quickly discover which function is the highest incoming
 * or outgoing contributor.
 */
export class StackSorter {
  static fromTop(array, maxDepth) {
    return new this(array, maxDepth, kForward).sorted();
  }

  static fromBottom(array, maxDepth) {
    return new this(array, maxDepth, kBackwards).sorted();
  }

  constructor(array, maxDepth, direction) {
    this.stacks = array;
    this.maxDepth = maxDepth;
    if (direction !== kForward && direction !== kBackwards) {
      throw new Error('Invalid direction');
    }
    this.direction = direction;
  }

  sorted() {
    const startLevel = this.direction == kForward ? 0 : this.maxDepth - 1;
    this._sort(0, this.stacks.length, startLevel);
    return this.stacks;
  }

  _sort(start, end, stackIndex) {
    if (stackIndex >= this.maxDepth || stackIndex < 0) return;
    const length = end - start;
    if (length <= 1) return;
    let counts;

    {
      const kNoFrame = -1;
      let bag = new Map();

      for (let i = start; i < end; i++) {
        let code = this.stacks[i][stackIndex] ?? kNoFrame;
        const count = bag.get(code) ?? 0;
        bag.set(code, count + 1);
      }

      // If all the frames are the same at the current stackIndex, check the
      // next stackIndex.
      if (bag.size === 1) {
        return this._sort(start, end, stackIndex + this.direction);
      }

      counts = Array.from(bag)
      counts.sort((a, b) => b[1] - a[1]);

      let currentIndex = start;
      // Reuse bag since we've copied out the counts;
      let insertionIndices = bag;
      for (let i = 0; i < counts.length; i++) {
        const pair = counts[i];
        const code = pair[0];
        const count = pair[1];
        insertionIndices.set(code, currentIndex);
        currentIndex += count;
      }
      // TODO do copy-less
      let stacksSegment = this.stacks.slice(start, end);
      for (let i = 0; i < length; i++) {
        const stack = stacksSegment[i];
        const entry = stack[stackIndex] ?? kNoFrame;
        const insertionIndex = insertionIndices.get(entry);
        if (!Number.isFinite(insertionIndex)) {
          throw 'Invalid insertionIndex: ' + insertionIndex;
        }
        if (insertionIndex < start || insertionIndex >= end) {
          throw 'Invalid insertionIndex: ' + insertionIndex;
        }
        this.stacks[insertionIndex] = stack;
        insertionIndices.set(entry, insertionIndex + 1);
      }
      // Free up variables before recursing.
      insertionIndices = bag = stacksSegment = undefined;
    }

    // Sort sub-segments
    let segmentStart = start;
    let segmentEnd = start;
    for (let i = 0; i < counts.length; i++) {
      const segmentLength = counts[i][1];
      segmentEnd = segmentStart + segmentLength - 1;
      this._sort(segmentStart, segmentEnd, stackIndex + this.direction);
      segmentStart = segmentEnd;
    }
  }
}

export class ProfileNode {
  constructor(codeEntry) {
    this.codeEntry = codeEntry;
    this.inCodeEntries = [];
    // [tick0, framePos0, tick1, framePos1, ...]
    this.ticksAndPosition = [];
    this.outCodeEntries = [];
    this._selfDuration = 0;
    this._totalDuration = 0;
  }

  stacksOut() {
    const slicedStacks = [];
    let maxDepth = 0;
    for (let i = 0; i < this.ticksAndPosition.length; i += 2) {
      // tick.stack = [topN, ..., top0, this.codeEntry, bottom0, ..., bottomN];
      const tick = this.ticksAndPosition[i];
      const stackIndex = this.ticksAndPosition[i + 1];
      // slice = [topN, ..., top0]
      const slice = tick.stack.slice(0, stackIndex);
      maxDepth = Math.max(maxDepth, slice.length);
      slicedStacks.push(slice);
    }
    // Before:
    //  stack1 = [f4, f3, f2, f1]
    //  stack2 = [f2, f1]
    // After:
    //  stack1 = [f4, f3, f2, f1]
    //  stack2 = [  ,   , f2, f1]
    for (let i = 0; i < slicedStacks.length; i++) {
      const stack = slicedStacks[i];
      const length = stack.length;
      if (length < maxDepth) {
        // Pad stacks at the beginning
        stack.splice(maxDepth - length, 0, undefined);
      }
    }
    // Start sorting at the bottom-most frame: top0 => topN / f1 => fN
    return StackSorter.fromBottom(slicedStacks, maxDepth);
  }

  stacksIn() {
    const slicedStacks = [];
    let maxDepth = 0;
    for (let i = 0; i < this.ticksAndPosition.length; i += 2) {
      // tick.stack = [topN, ..., top0, this.codeEntry, bottom0..., bottomN];
      const tick = this.ticksAndPosition[i];
      const stackIndex = this.ticksAndPosition[i + 1];
      // slice = [bottom0, ..., bottomN]
      const slice = tick.stack.slice(stackIndex + 1);
      maxDepth = Math.max(maxDepth, slice.length);
      slicedStacks.push(slice);
    }
    // Start storting at the top-most frame: bottom0 => bottomN
    return StackSorter.fromTop(slicedStacks, maxDepth);
  }

  startTime() {
    return this.ticksAndPosition[0].startTime;
  }

  endTime() {
    return this.ticksAndPosition[this.ticksAndPosition.length - 2].endTime;
  }

  duration() {
    return this.endTime() - this.startTime();
  }

  selfCount() {
    return this.totalCount() - this.outCodeEntries.length;
  }

  totalCount() {
    return this.ticksAndPosition.length / 2;
  }

  isLeaf() {
    return this.selfCount() == this.totalCount();
  }

  totalDuration() {
    let duration = 0;
    for (let entry of this.ticksAndPosition) duration += entry.duration;
    return duration;
  }

  selfDuration() {
    let duration = this.totalDuration();
    for (let entry of this.outCodeEntries) duration -= entry.duration;
    return duration;
  }
}

export class Flame {
  constructor(time, logEntry, depth, duration = -1) {
    this._time = time;
    this._logEntry = logEntry;
    this.depth = depth;
    this._duration = duration;
    this.parent = undefined;
    this.children = [];
  }

  static add(time, logEntry, stack, flames) {
    const depth = stack.length;
    const newFlame = new Flame(time, logEntry, depth);
    if (depth > 0) {
      const parent = stack[depth - 1];
      newFlame.parent = parent;
      parent.children.push(newFlame);
    }
    flames.push(newFlame);
    stack.push(newFlame);
  }

  stop(time) {
    if (this._duration !== -1) throw new Error('Already stopped');
    this._duration = time - this._time
  }

  get time() {
    return this._time;
  }

  get logEntry() {
    return this._logEntry;
  }

  get startTime() {
    return this._time;
  }

  get endTime() {
    return this._time + this._duration;
  }

  get duration() {
    return this._duration;
  }

  get type() {
    return TickLogEntry.extractCodeEntryType(this._logEntry?.entry);
  }

  get name() {
    return this._logEntry?.name;
  }
}

export class FlameBuilder {
  static forTime(ticks, reverseDepth) {
    return new this(ticks, true, reverseDepth);
  }

  static forTicks(ticks, reverseDepth = false) {
    return new this(ticks, false, reverseDepth);
  }

  constructor(ticks, useTime, reverseDepth) {
    this.maxDepth = 0;
    let tempFlames = this.flames = [];
    this.reverseDepth = reverseDepth;

    if (ticks.length == 0) return;

    if (!(ticks[0] instanceof TickLogEntry) && !Array.isArray(ticks[0])) {
      throw new Error(
          'Expected ticks array: `[TickLogEntry, ..]`, or raw stacks: `[[CodeEntry, ...], [...]]`');
    }
    // flameStack = [bottom, ..., top];
    const flameStack = [];
    let maxDepth = 0;
    const ticksLength = ticks.length;
    for (let tickIndex = 0; tickIndex < ticksLength; tickIndex++) {
      const tick = ticks[tickIndex];
      // tick is either a Tick log entry, or an Array
      const tickStack = tick.stack ?? tick;
      const tickStackLength = tickStack.length;
      const timeValue = useTime ? tick.time : tickIndex;
      maxDepth = Math.max(maxDepth, tickStackLength);
      // tick.stack = [top, .... , bottom];
      for (let stackIndex = tickStackLength - 1; stackIndex >= 0;
           stackIndex--) {
        const codeEntry = tickStack[stackIndex];
        // Assume that all higher stack entries are undefined as well.
        if (codeEntry === undefined) break;
        // codeEntry is either a CodeEntry or a raw pc.
        const logEntry = codeEntry?.logEntry ?? codeEntry;
        const flameStackIndex = tickStackLength - stackIndex - 1;
        if (flameStackIndex < flameStack.length) {
          if (flameStack[flameStackIndex].logEntry === logEntry) continue;
          // A lower frame changed, close all higher Flames.
          for (let k = flameStackIndex; k < flameStack.length; k++) {
            flameStack[k].stop(timeValue);
          }
          flameStack.length = flameStackIndex;
        }
        Flame.add(timeValue, logEntry, flameStack, tempFlames);
      }
      // Stop all Flames that are deeper nested than the current stack.
      if (tickStackLength < flameStack.length) {
        for (let k = tickStackLength; k < flameStack.length; k++) {
          flameStack[k].stop(timeValue);
        }
        flameStack.length = tickStackLength;
      }
    }
    this.maxDepth = maxDepth;

    const lastTime = useTime ? ticks[ticksLength - 1].time : ticksLength - 1;
    for (let k = 0; k < flameStack.length; k++) {
      flameStack[k].stop(lastTime);
    }
  }
}
