// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Timeline} from '../../timeline.mjs';
import {ToolTipEvent} from '../events.mjs';
import {arrayEquals, CSSColor, DOM, formatDurationMicros, SVG, V8CustomElement} from '../helper.mjs';

/** @template T */
export class Track {
  static continuous(array, color) {
    return new this(array, color, true);
  }

  static discrete(array, color) {
    return new this(array, color, false);
  }

  /** @param {T[]} logEntries */
  constructor(logEntries, color, isContinuous) {
    /** @type {Set<T>} */
    this.logEntries = new Set(logEntries);
    this.color = color;
    /** @type {bool} */
    this.isContinuous = isContinuous;
  }

  isEmpty() {
    return this.logEntries.size == 0;
  }

  /** @param {T} logEntry */
  hasEntry(logEntry) {
    return this.logEntries.has(logEntry);
  }

  static compare(left, right) {
    return left.equals(right);
  }

  equals(other) {
    if (!arrayEquals(
            Array.from(this.logEntries), Array.from(other.logEntries))) {
      return false;
    }
    if (this.color != other.color) return false;
    if (this.isContinuous != other.isContinuous) return false;
    return true;
  }
}

const kHorizontalPixels = 800;
const kMarginHeight = 5;
const kHeight = 40;

DOM.defineCustomElement('view/timeline/timeline-overview',
                        (templateText) =>
                            /** @template T */
                        class TimelineOverview extends V8CustomElement {
  /** @type {Timeline<T>} */
  _timeline;
  /** @type {Track[]} */
  _tracks = [];
  _timeToPixel = 1;
  /** @type {{entry:T, track:Track} => number} */
  _countCallback = (entry, track) => track.hasEntry(entry);

  constructor() {
    super(templateText);
    this._indicatorNode = this.$('#indicator');
    this._selectionNode = this.$('#selection');
    this._contentNode = this.$('#content');
    this._svgNode = this.$('#svg');
    this._svgNode.onmousemove = this._handleMouseMove.bind(this);
  }

  /**
   * @param {Timeline<T>} timeline
   */
  set timeline(timeline) {
    this._timeline = timeline;
    this._timeToPixel = kHorizontalPixels / this._timeline.duration();
  }

  /**
   * @param {Track[]} tracks
   */
  set tracks(tracks) {
    // TODO(cbruni): Allow updating the selection time-range independently from
    // the data.
    // if (arrayEquals(this._tracks, tracks, Track.compare)) return;
    this._tracks = tracks;
    this.requestUpdate();
  }

  /** @param {{entry:T, track:Track} => number} callback*/
  set countCallback(callback) {
    this._countCallback = callback;
  }

  _handleMouseMove(e) {
    const externalPixelToTime =
        this._timeline.duration() / this._svgNode.getBoundingClientRect().width;
    const timeToInternalPixel = kHorizontalPixels / this._timeline.duration();
    const xPos = e.offsetX;
    const timeMicros = xPos * externalPixelToTime;
    const maxTimeDistance = 2 * externalPixelToTime;
    this._setIndicatorPosition(timeMicros * timeToInternalPixel);
    let toolTipContent = this._findLogEntryAtTime(timeMicros, maxTimeDistance);
    if (!toolTipContent) {
      toolTipContent = `Time ${formatDurationMicros(timeMicros)}`;
    }
    this.dispatchEvent(
        new ToolTipEvent(toolTipContent, this._indicatorNode, e.ctrlKey));
  }

  _findLogEntryAtTime(time, maxTimeDistance) {
    const minTime = time - maxTimeDistance;
    const maxTime = time + maxTimeDistance;
    for (let track of this._tracks) {
      for (let entry of track.logEntries) {
        if (minTime <= entry.time && entry.time <= maxTime) return entry;
      }
    }
  }

  _setIndicatorPosition(x) {
    this._indicatorNode.setAttribute('x1', x);
    this._indicatorNode.setAttribute('x2', x);
  }

  _update() {
    const fragment = new DocumentFragment();
    this._tracks.forEach((track, index) => {
      if (!track.isEmpty()) {
        fragment.appendChild(this._renderTrack(track, index));
      }
    });
    DOM.removeAllChildren(this._contentNode);
    this._contentNode.appendChild(fragment);
    this._setIndicatorPosition(-10);
    this._updateSelection();
  }

  _renderTrack(track, index) {
    if (track.isContinuous) return this._renderContinuousTrack(track, index);
    return this._renderDiscreteTrack(track, index);
  }

  _renderContinuousTrack(track, index) {
    const freq = new Frequency(this._timeline);
    freq.collect(track, this._countCallback);
    const path = SVG.path('continuousTrack');
    path.setAttribute('d', freq.toSVG(kHeight));
    path.setAttribute('fill', track.color);
    if (index != 0) path.setAttribute('mask', `url(#mask${index})`)
      return path;
  }

  _renderDiscreteTrack(track) {
    const group = SVG.g();
    for (let entry of track.logEntries) {
      const x = entry.time * this._timeToPixel;
      const kWidth = 2;
      const rect = SVG.rect('marker');
      rect.setAttribute('x', x - (kWidth / 2));
      rect.setAttribute('fill', track.color);
      rect.data = entry;
      group.appendChild(rect);
    }
    return group;
  }

  _updateSelection() {
    let startTime = -10;
    let duration = 0;
    if (this._timeline) {
      startTime = this._timeline.selectionOrSelf.startTime;
      duration = this._timeline.selectionOrSelf.duration();
    }
    this._selectionNode.setAttribute('x', startTime * this._timeToPixel);
    this._selectionNode.setAttribute('width', duration * this._timeToPixel);
  }
});

const kernel = smoothingKernel(50);
function smoothingKernel(size) {
  const kernel = new Float32Array(size);
  const mid = (size - 1) / 2;
  const stddev = size / 10;
  for (let i = mid; i < size; i++) {
    const x = i - (mid | 0);
    const value =
        Math.exp(-(x ** 2 / 2 / stddev)) / (stddev * Math.sqrt(2 * Math.PI));
    kernel[Math.ceil(x + mid)] = value;
    kernel[Math.floor(mid - x)] = value;
  }
  return kernel;
}

class Frequency {
  constructor(timeline) {
    this._size = kHorizontalPixels;
    this._timeline = timeline;
    this._data = new Int16Array(this._size + kernel.length);
    this._max = undefined;
    this._smoothenedData = undefined;
  }

  collect(track, sumFn) {
    const kernelRadius = (kernel.length / 2) | 0;
    let count = 0;
    let dataIndex = kernelRadius;
    const timeDelta = this._timeline.duration() / this._size;
    let nextTime = this._timeline.startTime + timeDelta;
    const values = this._timeline.values;
    const length = values.length;
    let i = 0;
    while (i < length && dataIndex < this._data.length) {
      const tick = values[i];
      if (tick.time < nextTime) {
        count += sumFn(tick, track);
        i++;
      } else {
        this._data[dataIndex] = count;
        nextTime += timeDelta;
        dataIndex++;
        count = 0;
      }
    }
    this._data[dataIndex] = count;
  }

  max() {
    if (this._max !== undefined) return this._max;
    let max = 0;
    this._smoothenedData = new Float32Array(this._size);
    for (let start = 0; start < this._size; start++) {
      let value = 0;
      for (let i = 0; i < kernel.length; i++) {
        value += this._data[start + i] * kernel[i];
      }
      this._smoothenedData[start] = value;
      max = Math.max(max, value);
    }
    this._max = max;
    return this._max;
  }

  toSVG(height) {
    const vScale = height / this.max();
    const initialY = height;
    const buffer = [
      'M 0',
      initialY,
    ];
    let prevY = initialY;
    let usedPrevY = false;
    for (let i = 0; i < this._size; i++) {
      const y = height - (this._smoothenedData[i] * vScale) | 0;
      if (y == prevY) {
        usedPrevY = false;
        continue;
      }
      if (!usedPrevY) buffer.push('L', i - 1, prevY);
      buffer.push('L', i, y);
      prevY = y;
      usedPrevY = true;
    }
    if (!usedPrevY) buffer.push('L', this._size - 1, prevY);
    buffer.push('L', this._size - 1, initialY);
    buffer.push('Z');
    return buffer.join(' ');
  }
}
