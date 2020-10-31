// Copyright (C) 2019 The Android Open Source Project
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

import {searchEq, searchRange} from '../../base/binary_search';
import {assertTrue} from '../../base/logging';
import {TrackState} from '../../common/state';
import {checkerboardExcept} from '../../frontend/checkerboard';
import {colorForThread, colorForTid} from '../../frontend/colorizer';
import {globals} from '../../frontend/globals';
import {Track} from '../../frontend/track';
import {trackRegistry} from '../../frontend/track_registry';

import {
  Config,
  Data,
  PROCESS_SCHEDULING_TRACK_KIND,
  SliceData,
  SummaryData
} from './common';

const MARGIN_TOP = 5;
const RECT_HEIGHT = 30;
const TRACK_HEIGHT = MARGIN_TOP * 2 + RECT_HEIGHT;
const SUMMARY_HEIGHT = TRACK_HEIGHT - MARGIN_TOP;

class ProcessSchedulingTrack extends Track<Config, Data> {
  static readonly kind = PROCESS_SCHEDULING_TRACK_KIND;
  static create(trackState: TrackState): ProcessSchedulingTrack {
    return new ProcessSchedulingTrack(trackState);
  }

  private mouseXpos?: number;
  private utidHoveredInThisTrack = -1;

  constructor(trackState: TrackState) {
    super(trackState);
  }

  getHeight(): number {
    return TRACK_HEIGHT;
  }

  renderCanvas(ctx: CanvasRenderingContext2D): void {
    // TODO: fonts and colors should come from the CSS and not hardcoded here.
    const {timeScale, visibleWindowTime} = globals.frontendLocalState;
    const data = this.data();

    if (data === undefined) return;  // Can't possibly draw anything.

    // If the cached trace slices don't fully cover the visible time range,
    // show a gray rectangle with a "Loading..." label.
    checkerboardExcept(
        ctx,
        this.getHeight(),
        timeScale.timeToPx(visibleWindowTime.start),
        timeScale.timeToPx(visibleWindowTime.end),
        timeScale.timeToPx(data.start),
        timeScale.timeToPx(data.end));

    if (data.kind === 'summary') {
      this.renderSummary(ctx, data);
    } else if (data.kind === 'slice') {
      this.renderSlices(ctx, data);
    }
  }

  renderSummary(ctx: CanvasRenderingContext2D, data: SummaryData): void {
    const {timeScale, visibleWindowTime} = globals.frontendLocalState;
    const startPx = Math.floor(timeScale.timeToPx(visibleWindowTime.start));
    const bottomY = TRACK_HEIGHT;

    let lastX = startPx;
    let lastY = bottomY;

    const color = colorForTid(this.config.pidForColor);
    ctx.fillStyle = `hsl(${color.h}, ${color.s}%, ${color.l}%)`;
    ctx.beginPath();
    ctx.moveTo(lastX, lastY);
    for (let i = 0; i < data.utilizations.length; i++) {
      const utilization = data.utilizations[i];
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

  renderSlices(ctx: CanvasRenderingContext2D, data: SliceData): void {
    const {timeScale, visibleWindowTime} = globals.frontendLocalState;
    assertTrue(data.starts.length === data.ends.length);
    assertTrue(data.starts.length === data.utids.length);

    const cpuTrackHeight = Math.floor(RECT_HEIGHT / data.maxCpu);

    for (let i = 0; i < data.starts.length; i++) {
      const tStart = data.starts[i];
      const tEnd = data.ends[i];
      const utid = data.utids[i];
      const cpu = data.cpus[i];
      if (tEnd <= visibleWindowTime.start || tStart >= visibleWindowTime.end) {
        continue;
      }
      const rectStart = timeScale.timeToPx(tStart);
      const rectEnd = timeScale.timeToPx(tEnd);
      const rectWidth = rectEnd - rectStart;
      if (rectWidth < 0.3) continue;

      const threadInfo = globals.threads.get(utid);
      const pid = (threadInfo ? threadInfo.pid : -1) || -1;

      const isHovering = globals.frontendLocalState.hoveredUtid !== -1;
      const isThreadHovered = globals.frontendLocalState.hoveredUtid === utid;
      const isProcessHovered = globals.frontendLocalState.hoveredPid === pid;
      const color = colorForThread(threadInfo);
      if (isHovering && !isThreadHovered) {
        if (!isProcessHovered) {
          color.l = 90;
          color.s = 0;
        } else {
          color.l = Math.min(color.l + 30, 80);
          color.s -= 20;
        }
      } else {
        color.l = Math.min(color.l + 10, 60);
        color.s -= 20;
      }
      ctx.fillStyle = `hsl(${color.h}, ${color.s}%, ${color.l}%)`;
      const y = MARGIN_TOP + cpuTrackHeight * cpu + cpu;
      ctx.fillRect(rectStart, y, rectEnd - rectStart, cpuTrackHeight);
    }

    const hoveredThread = globals.threads.get(this.utidHoveredInThisTrack);
    if (hoveredThread !== undefined && this.mouseXpos !== undefined) {
      const tidText = `T: ${hoveredThread.threadName} [${hoveredThread.tid}]`;
      if (hoveredThread.pid) {
        const pidText = `P: ${hoveredThread.procName} [${hoveredThread.pid}]`;
        this.drawTrackHoverTooltip(ctx, this.mouseXpos, pidText, tidText);
      } else {
        this.drawTrackHoverTooltip(ctx, this.mouseXpos, tidText);
      }
    }
  }

  onMouseMove({x, y}: {x: number, y: number}) {
    const data = this.data();
    this.mouseXpos = x;
    if (data === undefined || data.kind === 'summary') return;
    if (y < MARGIN_TOP || y > MARGIN_TOP + RECT_HEIGHT) {
      this.utidHoveredInThisTrack = -1;
      globals.frontendLocalState.setHoveredUtidAndPid(-1, -1);
      return;
    }

    const cpuTrackHeight = Math.floor(RECT_HEIGHT / data.maxCpu);
    const cpu = Math.floor((y - MARGIN_TOP) / (cpuTrackHeight + 1));
    const {timeScale} = globals.frontendLocalState;
    const t = timeScale.pxToTime(x);

    const [i, j] = searchRange(data.starts, t, searchEq(data.cpus, cpu));
    if (i === j || i >= data.starts.length || t > data.ends[i]) {
      this.utidHoveredInThisTrack = -1;
      globals.frontendLocalState.setHoveredUtidAndPid(-1, -1);
      return;
    }

    const utid = data.utids[i];
    this.utidHoveredInThisTrack = utid;
    const threadInfo = globals.threads.get(utid);
    const pid = threadInfo ? (threadInfo.pid ? threadInfo.pid : -1) : -1;
    globals.frontendLocalState.setHoveredUtidAndPid(utid, pid);
  }

  onMouseOut() {
    this.utidHoveredInThisTrack = -1;
    globals.frontendLocalState.setHoveredUtidAndPid(-1, -1);
    this.mouseXpos = 0;
  }
}

trackRegistry.register(ProcessSchedulingTrack);
