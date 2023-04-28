// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Flame, FlameBuilder} from '../../profiling.mjs';
import {Timeline} from '../../timeline.mjs';
import {delay, DOM, SVG} from '../helper.mjs';

import {TimelineTrackStackedBase} from './timeline-track-stacked-base.mjs'

DOM.defineCustomElement(
    'view/timeline/timeline-track', 'timeline-track-tick',
    (templateText) => class TimelineTrackTick extends TimelineTrackStackedBase {
      constructor() {
        super(templateText);
        this._annotations = new Annotations(this);
      }

      _prepareDrawableItems() {
        const flameBuilder = FlameBuilder.forTime(this._timeline.values, true);
        this._drawableItems = new Timeline(Flame, flameBuilder.flames);
        this._annotations.flames = this._drawableItems;
        this._adjustStackDepth(flameBuilder.maxDepth);
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
