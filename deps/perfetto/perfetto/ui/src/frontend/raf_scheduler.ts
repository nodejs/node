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

import {assertTrue} from '../base/logging';

import {globals} from './globals';

import {
  debugNow,
  measure,
  perfDebug,
  perfDisplay,
  RunningStatistics,
  runningStatStr
} from './perf';

function statTableHeader() {
  return m(
      'tr',
      m('th', ''),
      m('th', 'Last (ms)'),
      m('th', 'Avg (ms)'),
      m('th', 'Avg-10 (ms)'), );
}

function statTableRow(title: string, stat: RunningStatistics) {
  return m(
      'tr',
      m('td', title),
      m('td', stat.last.toFixed(2)),
      m('td', stat.mean.toFixed(2)),
      m('td', stat.bufferMean.toFixed(2)), );
}

export type ActionCallback = (nowMs: number) => void;
export type RedrawCallback = (nowMs: number) => void;

// This class orchestrates all RAFs in the UI. It ensures that there is only
// one animation frame handler overall and that callbacks are called in
// predictable order. There are two types of callbacks here:
// - actions (e.g. pan/zoon animations), which will alter the "fast"
//  (main-thread-only) state (e.g. update visible time bounds @ 60 fps).
// - redraw callbacks that will repaint canvases.
// This class guarantees that, on each frame, redraw callbacks are called after
// all action callbacks.
export class RafScheduler {
  private actionCallbacks = new Set<ActionCallback>();
  private canvasRedrawCallbacks = new Set<RedrawCallback>();
  private _syncDomRedraw: RedrawCallback = _ => {};
  private hasScheduledNextFrame = false;
  private requestedFullRedraw = false;
  private isRedrawing = false;
  private _shutdown = false;

  private perfStats = {
    rafActions: new RunningStatistics(),
    rafCanvas: new RunningStatistics(),
    rafDom: new RunningStatistics(),
    rafTotal: new RunningStatistics(),
    domRedraw: new RunningStatistics(),
  };

  start(cb: ActionCallback) {
    this.actionCallbacks.add(cb);
    this.maybeScheduleAnimationFrame();
  }

  stop(cb: ActionCallback) {
    this.actionCallbacks.delete(cb);
  }

  addRedrawCallback(cb: RedrawCallback) {
    this.canvasRedrawCallbacks.add(cb);
  }

  removeRedrawCallback(cb: RedrawCallback) {
    this.canvasRedrawCallbacks.delete(cb);
  }

  scheduleRedraw() {
    this.maybeScheduleAnimationFrame(true);
  }

  shutdown() {
    this._shutdown = true;
  }

  set domRedraw(cb: RedrawCallback|null) {
    this._syncDomRedraw = cb || (_ => {});
  }

  scheduleFullRedraw() {
    this.requestedFullRedraw = true;
    this.maybeScheduleAnimationFrame(true);
  }

  syncDomRedraw(nowMs: number) {
    const redrawStart = debugNow();
    this._syncDomRedraw(nowMs);
    if (perfDebug()) {
      this.perfStats.domRedraw.addValue(debugNow() - redrawStart);
    }
  }

  private syncCanvasRedraw(nowMs: number) {
    const redrawStart = debugNow();
    if (this.isRedrawing) return;
    globals.frontendLocalState.clearVisibleTracks();
    this.isRedrawing = true;
    for (const redraw of this.canvasRedrawCallbacks) redraw(nowMs);
    this.isRedrawing = false;
    globals.frontendLocalState.sendVisibleTracks();
    if (perfDebug()) {
      this.perfStats.rafCanvas.addValue(debugNow() - redrawStart);
    }
  }

  private maybeScheduleAnimationFrame(force = false) {
    if (this.hasScheduledNextFrame) return;
    if (this.actionCallbacks.size !== 0 || force) {
      this.hasScheduledNextFrame = true;
      window.requestAnimationFrame(this.onAnimationFrame.bind(this));
    }
  }

  private onAnimationFrame(nowMs: number) {
    if (this._shutdown) return;
    const rafStart = debugNow();
    this.hasScheduledNextFrame = false;

    const doFullRedraw = this.requestedFullRedraw;
    this.requestedFullRedraw = false;

    const actionTime = measure(() => {
      for (const action of this.actionCallbacks) action(nowMs);
    });

    const domTime = measure(() => {
      if (doFullRedraw) this.syncDomRedraw(nowMs);
    });
    const canvasTime = measure(() => this.syncCanvasRedraw(nowMs));

    const totalRafTime = debugNow() - rafStart;
    this.updatePerfStats(actionTime, domTime, canvasTime, totalRafTime);
    perfDisplay.renderPerfStats();

    this.maybeScheduleAnimationFrame();
  }

  private updatePerfStats(
      actionsTime: number, domTime: number, canvasTime: number,
      totalRafTime: number) {
    if (!perfDebug()) return;
    this.perfStats.rafActions.addValue(actionsTime);
    this.perfStats.rafDom.addValue(domTime);
    this.perfStats.rafCanvas.addValue(canvasTime);
    this.perfStats.rafTotal.addValue(totalRafTime);
  }

  renderPerfStats() {
    assertTrue(perfDebug());
    return m(
        'div',
        m('div',
          [
            m('button',
              {onclick: () => this.scheduleRedraw()},
              'Do Canvas Redraw'),
            '   |   ',
            m('button',
              {onclick: () => this.scheduleFullRedraw()},
              'Do Full Redraw'),
          ]),
        m('div',
          'Raf Timing ' +
              '(Total may not add up due to imprecision)'),
        m('table',
          statTableHeader(),
          statTableRow('Actions', this.perfStats.rafActions),
          statTableRow('Dom', this.perfStats.rafDom),
          statTableRow('Canvas', this.perfStats.rafCanvas),
          statTableRow('Total', this.perfStats.rafTotal), ),
        m('div',
          'Dom redraw: ' +
              `Count: ${this.perfStats.domRedraw.count} | ` +
              runningStatStr(this.perfStats.domRedraw)), );
  }
}
