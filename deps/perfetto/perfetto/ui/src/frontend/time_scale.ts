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

import {TimeSpan} from '../common/time';

const MAX_ZOOM_SPAN_SEC = 1e-4;  // 0.1 ms.

/**
 * Defines a mapping between number and seconds for the entire application.
 * Linearly scales time values from boundsMs to pixel values in boundsPx and
 * back.
 */
export class TimeScale {
  private timeBounds: TimeSpan;
  private _startPx: number;
  private _endPx: number;
  private secPerPx = 0;

  constructor(timeBounds: TimeSpan, boundsPx: [number, number]) {
    this.timeBounds = timeBounds;
    this._startPx = boundsPx[0];
    this._endPx = boundsPx[1];
    this.updateSlope();
  }

  private updateSlope() {
    this.secPerPx = this.timeBounds.duration / (this._endPx - this._startPx);
  }

  deltaTimeToPx(time: number): number {
    return Math.round(time / this.secPerPx);
  }

  timeToPx(time: number): number {
    return this._startPx + (time - this.timeBounds.start) / this.secPerPx;
  }

  pxToTime(px: number): number {
    return this.timeBounds.start + (px - this._startPx) * this.secPerPx;
  }

  deltaPxToDuration(px: number): number {
    return px * this.secPerPx;
  }

  setTimeBounds(timeBounds: TimeSpan) {
    this.timeBounds = timeBounds;
    this.updateSlope();
  }

  setLimitsPx(pxStart: number, pxEnd: number) {
    this._startPx = pxStart;
    this._endPx = pxEnd;
    this.updateSlope();
  }

  timeInBounds(time: number): boolean {
    return this.timeBounds.isInBounds(time);
  }

  get startPx(): number {
    return this._startPx;
  }

  get endPx(): number {
    return this._endPx;
  }
}

export function computeZoom(
    scale: TimeScale, span: TimeSpan, zoomFactor: number, zoomPx: number):
    TimeSpan {
  const startPx = scale.startPx;
  const endPx = scale.endPx;
  const deltaPx = endPx - startPx;
  const deltaTime = span.end - span.start;
  const newDeltaTime = Math.max(deltaTime * zoomFactor, MAX_ZOOM_SPAN_SEC);
  const clampedZoomPx = Math.max(startPx, Math.min(endPx, zoomPx));
  const zoomTime = scale.pxToTime(clampedZoomPx);
  const r = (clampedZoomPx - startPx) / deltaPx;
  const newStartTime = zoomTime - newDeltaTime * r;
  const newEndTime = newStartTime + newDeltaTime;
  return new TimeSpan(newStartTime, newEndTime);
}
