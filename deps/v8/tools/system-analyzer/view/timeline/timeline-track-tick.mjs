// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TickLogEntry} from '../../log/tick.mjs';
import {Timeline} from '../../timeline.mjs';
import {delay, DOM, SVG} from '../helper.mjs';

import {TimelineTrackStackedBase} from './timeline-track-stacked-base.mjs'

class Flame {
  constructor(time, logEntry, depth) {
    this._time = time;
    this._logEntry = logEntry;
    this.depth = depth;
    this._duration = -1;
    this.parent = undefined;
    this.children = [];
  }

  static add(time, logEntry, stack, flames) {
    const depth = stack.length;
    const newFlame = new Flame(time, logEntry, depth)
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
}

DOM.defineCustomElement(
    'view/timeline/timeline-track', 'timeline-track-tick',
    (templateText) => class TimelineTrackTick extends TimelineTrackStackedBase {
      constructor() {
        super(templateText);
        this._annotations = new Annotations(this);
      }

      _prepareDrawableItems() {
        const tmpFlames = [];
        // flameStack = [bottom, ..., top];
        const flameStack = [];
        const ticks = this._timeline.values;
        let maxDepth = 0;
        for (let tickIndex = 0; tickIndex < ticks.length; tickIndex++) {
          const tick = ticks[tickIndex];
          const tickStack = tick.stack;
          maxDepth = Math.max(maxDepth, tickStack.length);
          // tick.stack = [top, .... , bottom];
          for (let stackIndex = tickStack.length - 1; stackIndex >= 0;
               stackIndex--) {
            const codeEntry = tickStack[stackIndex];
            // codeEntry is either a CodeEntry or a raw pc.
            const logEntry = codeEntry?.logEntry;
            const flameStackIndex = tickStack.length - stackIndex - 1;
            if (flameStackIndex < flameStack.length) {
              if (flameStack[flameStackIndex].logEntry === logEntry) continue;
              for (let k = flameStackIndex; k < flameStack.length; k++) {
                flameStack[k].stop(tick.time);
              }
              flameStack.length = flameStackIndex;
            }
            Flame.add(tick.time, logEntry, flameStack, tmpFlames);
          }
          if (tickStack.length < flameStack.length) {
            for (let k = tickStack.length; k < flameStack.length; k++) {
              flameStack[k].stop(tick.time);
            }
            flameStack.length = tickStack.length;
          }
        }
        const lastTime = ticks[ticks.length - 1].time;
        for (let k = 0; k < flameStack.length; k++) {
          flameStack[k].stop(lastTime);
        }
        this._drawableItems = new Timeline(Flame, tmpFlames);
        this._annotations.flames = this._drawableItems;
        this._adjustStackDepth(maxDepth);
      }

      _drawAnnotations(logEntry, time) {
        if (time === undefined) {
          time = this.relativePositionToTime(this._timelineScrollLeft);
        }
        this._annotations.update(logEntry, time);
      }

      _drawableItemToLogEntry(flame) {
        const logEntry = flame?.logEntry;
        if (logEntry === undefined || typeof logEntry == 'number')
          return undefined;
        return logEntry;
      }
    })

class Annotations {
  _flames;
  _logEntry;
  _buffer;

  constructor(track) {
    this._track = track;
  }

  set flames(flames) {
    this._flames = flames;
  }

  get _node() {
    return this._track.timelineAnnotationsNode;
  }

  async update(logEntry, time) {
    if (this._logEntry == logEntry) return;
    this._logEntry = logEntry;
    this._node.innerHTML = '';
    if (logEntry === undefined) return;
    this._buffer = '';
    const start = this._flames.find(time);
    let offset = 0;
    // Draw annotations gradually outwards starting form the given time.
    let deadline = performance.now() + 100;
    for (let range = 0; range < this._flames.length; range += 10000) {
      this._markFlames(start - range, start - offset);
      this._markFlames(start + offset, start + range);
      offset = range;
      if ((navigator?.scheduling?.isInputPending({includeContinuous: true}) ??
           false) ||
          performance.now() >= deadline) {
        // Yield if we have to handle an input event, or we're out of time.
        await delay(50);
        // Abort if we started another update asynchronously.
        if (this._logEntry != logEntry) return;

        deadline = performance.now() + 100;
      }
      this._drawBuffer();
    }
    this._drawBuffer();
  }

  _markFlames(start, end) {
    const rawFlames = this._flames.values;
    if (start < 0) start = 0;
    if (end > rawFlames.length) end = rawFlames.length;
    const logEntry = this._logEntry;
    // Also compare against the function, if any.
    const func = logEntry.entry?.func ?? -1;
    for (let i = start; i < end; i++) {
      const flame = rawFlames[i];
      const flameLogEntry = flame.logEntry;
      if (!flameLogEntry) continue;
      if (flameLogEntry !== logEntry) {
        if (flameLogEntry.entry?.func !== func) continue;
      }
      this._buffer += this._track._drawItem(flame, i, true);
    }
  }

  _drawBuffer() {
    if (this._buffer.length == 0) return;
    const svg = SVG.svg();
    svg.innerHTML = this._buffer;
    this._node.appendChild(svg);
    this._buffer = '';
  }
}
