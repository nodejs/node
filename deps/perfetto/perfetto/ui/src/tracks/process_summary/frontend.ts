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

import {TrackState} from '../../common/state';
import {checkerboardExcept} from '../../frontend/checkerboard';
import {colorForTid} from '../../frontend/colorizer';
import {globals} from '../../frontend/globals';
import {Track} from '../../frontend/track';
import {trackRegistry} from '../../frontend/track_registry';

import {
  Config,
  Data,
  PROCESS_SUMMARY_TRACK,
} from './common';

const MARGIN_TOP = 5;
const RECT_HEIGHT = 30;
const TRACK_HEIGHT = MARGIN_TOP * 2 + RECT_HEIGHT;
const SUMMARY_HEIGHT = TRACK_HEIGHT - MARGIN_TOP;

class ProcessSummaryTrack extends Track<Config, Data> {
  static readonly kind = PROCESS_SUMMARY_TRACK;
  static create(trackState: TrackState): ProcessSummaryTrack {
    return new ProcessSummaryTrack(trackState);
  }

  constructor(trackState: TrackState) {
    super(trackState);
  }

  getHeight(): number {
    return TRACK_HEIGHT;
  }

  renderCanvas(ctx: CanvasRenderingContext2D): void {
    const {timeScale, visibleWindowTime} = globals.frontendLocalState;
    const data = this.data();
    if (data === undefined) return;  // Can't possibly draw anything.

    checkerboardExcept(
        ctx,
        this.getHeight(),
        timeScale.timeToPx(visibleWindowTime.start),
        timeScale.timeToPx(visibleWindowTime.end),
        timeScale.timeToPx(data.start),
        timeScale.timeToPx(data.end));

    this.renderSummary(ctx, data);
  }

  // TODO(dproy): Dedup with CPU slices.
  renderSummary(ctx: CanvasRenderingContext2D, data: Data): void {
    const {timeScale, visibleWindowTime} = globals.frontendLocalState;
    const startPx = Math.floor(timeScale.timeToPx(visibleWindowTime.start));
    const bottomY = TRACK_HEIGHT;

    let lastX = startPx;
    let lastY = bottomY;

    // TODO(hjd): Dedupe this math.
    const color = colorForTid(this.config.pidForColor);
    color.l = Math.min(color.l + 10, 60);
    color.s -= 20;

    ctx.fillStyle = `hsl(${color.h}, ${color.s}%, ${color.l}%)`;
    ctx.beginPath();
    ctx.moveTo(lastX, lastY);
    for (let i = 0; i < data.utilizations.length; i++) {
      // TODO(dproy): Investigate why utilization is > 1 sometimes.
      const utilization = Math.min(data.utilizations[i], 1);
      const startTime = i * data.bucketSizeSeconds + data.start;

      lastX = Math.floor(timeScale.timeToPx(startTime));

      ctx.lineTo(lastX, lastY);
      lastY = MARGIN_TOP + Math.round(SUMMARY_HEIGHT * (1 - utilization));
      ctx.lineTo(lastX, lastY);
    }
    ctx.lineTo(lastX, bottomY);
    ctx.closePath();
    ctx.fill();
  }
}

trackRegistry.register(ProcessSummaryTrack);
