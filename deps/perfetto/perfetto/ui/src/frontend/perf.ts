
// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import * as m from 'mithril';

import {globals} from './globals';
import {PanelContainer} from './panel_container';

/**
 * Shorthand for if globals perf debug mode is on.
 */
export const perfDebug = () => globals.frontendLocalState.perfDebug;

/**
 * Returns performance.now() if perfDebug is enabled, otherwise 0.
 * This is needed because calling performance.now is generally expensive
 * and should not be done for every frame.
 */
export const debugNow = () => perfDebug() ? performance.now() : 0;

/**
 * Returns execution time of |fn| if perf debug mode is on. Returns 0 otherwise.
 */
export function measure(fn: () => void): number {
  const start = debugNow();
  fn();
  return debugNow() - start;
}

/**
 * Stores statistics about samples, and keeps a fixed size buffer of most recent
 * samples.
 */
export class RunningStatistics {
  private _count = 0;
  private _mean = 0;
  private _lastValue = 0;

  private buffer: number[] = [];

  constructor(private _maxBufferSize = 10) {}

  addValue(value: number) {
    this._lastValue = value;
    this.buffer.push(value);
    if (this.buffer.length > this._maxBufferSize) {
      this.buffer.shift();
    }
    this._mean = (this._mean * this._count + value) / (this._count + 1);
    this._count++;
  }

  get mean() {
    return this._mean;
  }
  get count() {
    return this._count;
  }
  get bufferMean() {
    return this.buffer.reduce((sum, v) => sum + v, 0) / this.buffer.length;
  }
  get bufferSize() {
    return this.buffer.length;
  }
  get maxBufferSize() {
    return this._maxBufferSize;
  }
  get last() {
    return this._lastValue;
  }
}

/**
 * Returns a summary string representation of a RunningStatistics object.
 */
export function runningStatStr(stat: RunningStatistics) {
  return `Last: ${stat.last.toFixed(2)}ms | ` +
      `Avg: ${stat.mean.toFixed(2)}ms | ` +
      `Avg${stat.maxBufferSize}: ${stat.bufferMean.toFixed(2)}ms`;
}

/**
 * Globals singleton class that renders performance stats for the whole app.
 */
class PerfDisplay {
  private containers: PanelContainer[] = [];
  addContainer(container: PanelContainer) {
    this.containers.push(container);
  }

  removeContainer(container: PanelContainer) {
    const i = this.containers.indexOf(container);
    this.containers.splice(i, 1);
  }

  renderPerfStats() {
    if (!perfDebug()) return;
    const perfDisplayEl = document.querySelector('.perf-stats');
    if (!perfDisplayEl) return;
    m.render(perfDisplayEl, [
      m('section', globals.rafScheduler.renderPerfStats()),
      m('button.close-button',
        {
          onclick: () => globals.frontendLocalState.togglePerfDebug(),
        },
        m('i.material-icons', 'close')),
      this.containers.map((c, i) => m('section', c.renderPerfStats(i)))
    ]);
  }
}

export const perfDisplay = new PerfDisplay();
