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

import {search, searchEq} from '../../base/binary_search';
import {assertTrue} from '../../base/logging';
import {Actions} from '../../common/actions';
import {cropText, drawDoubleHeadedArrow} from '../../common/canvas_utils';
import {TrackState} from '../../common/state';
import {timeToString} from '../../common/time';
import {checkerboardExcept} from '../../frontend/checkerboard';
import {colorForThread, hueForCpu} from '../../frontend/colorizer';
import {globals} from '../../frontend/globals';
import {Track} from '../../frontend/track';
import {trackRegistry} from '../../frontend/track_registry';

import {
  Config,
  CPU_SLICE_TRACK_KIND,
  Data,
  SliceData,
  SummaryData
} from './common';

const MARGIN_TOP = 3;
const RECT_HEIGHT = 24;
const TRACK_HEIGHT = MARGIN_TOP * 2 + RECT_HEIGHT;
const SUMMARY_HEIGHT = TRACK_HEIGHT - MARGIN_TOP;

class CpuSliceTrack extends Track<Config, Data> {
  static readonly kind = CPU_SLICE_TRACK_KIND;
  static create(trackState: TrackState): CpuSliceTrack {
    return new CpuSliceTrack(trackState);
  }

  private mouseXpos?: number;
  private hue: number;
  private utidHoveredInThisTrack = -1;

  constructor(trackState: TrackState) {
    super(trackState);
    this.hue = hueForCpu(this.config.cpu);
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

    ctx.fillStyle = `hsl(${this.hue}, 50%, 60%)`;
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

    ctx.textAlign = 'center';
    ctx.font = '12px Roboto Condensed';
    const charWidth = ctx.measureText('dbpqaouk').width / 8;

    for (let i = 0; i < data.starts.length; i++) {
      const tStart = data.starts[i];
      const tEnd = data.ends[i];
      const utid = data.utids[i];
      if (tEnd <= visibleWindowTime.start || tStart >= visibleWindowTime.end) {
        continue;
      }
      const rectStart = timeScale.timeToPx(tStart);
      const rectEnd = timeScale.timeToPx(tEnd);
      const rectWidth = Math.max(1, rectEnd - rectStart);
      const threadInfo = globals.threads.get(utid);

      // TODO: consider de-duplicating this code with the copied one from
      // chrome_slices/frontend.ts.
      let title = `[utid:${utid}]`;
      let subTitle = '';
      let pid = -1;
      if (threadInfo) {
        if (threadInfo.pid) {
          pid = threadInfo.pid;
          const procName = threadInfo.procName || '';
          title = `${procName} [${threadInfo.pid}]`;
          subTitle = `${threadInfo.threadName} [${threadInfo.tid}]`;
        } else {
          title = `${threadInfo.threadName} [${threadInfo.tid}]`;
        }
      }

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
      ctx.fillRect(rectStart, MARGIN_TOP, rectWidth, RECT_HEIGHT);

      // Don't render text when we have less than 5px to play with.
      if (rectWidth < 5) continue;

      title = cropText(title, charWidth, rectWidth);
      subTitle = cropText(subTitle, charWidth, rectWidth);
      const rectXCenter = rectStart + rectWidth / 2;
      ctx.fillStyle = '#fff';
      ctx.font = '12px Roboto Condensed';
      ctx.fillText(title, rectXCenter, MARGIN_TOP + RECT_HEIGHT / 2 - 1);
      ctx.fillStyle = 'rgba(255, 255, 255, 0.6)';
      ctx.font = '10px Roboto Condensed';
      ctx.fillText(subTitle, rectXCenter, MARGIN_TOP + RECT_HEIGHT / 2 + 9);
    }

    const selection = globals.state.currentSelection;
    const details = globals.sliceDetails;
    if (selection !== null && selection.kind === 'SLICE') {
      const [startIndex, endIndex] = searchEq(data.ids, selection.id);
      if (startIndex !== endIndex) {
        const tStart = data.starts[startIndex];
        const tEnd = data.ends[startIndex];
        const utid = data.utids[startIndex];
        const color = colorForThread(globals.threads.get(utid));
        const rectStart = timeScale.timeToPx(tStart);
        const rectEnd = timeScale.timeToPx(tEnd);
        const rectWidth = Math.max(1, rectEnd - rectStart);

        // Draw a rectangle around the slice that is currently selected.
        ctx.strokeStyle = `hsl(${color.h}, ${color.s}%, 30%)`;
        ctx.beginPath();
        ctx.lineWidth = 3;
        ctx.strokeRect(rectStart, MARGIN_TOP - 1.5, rectWidth, RECT_HEIGHT + 3);
        ctx.closePath();
        // Draw arrow from wakeup time of current slice.
        if (details.wakeupTs) {
          const wakeupPos = timeScale.timeToPx(details.wakeupTs);
          const latencyWidth = rectStart - wakeupPos;
          drawDoubleHeadedArrow(
              ctx,
              wakeupPos,
              MARGIN_TOP + RECT_HEIGHT,
              latencyWidth,
              latencyWidth >= 20);
          // Latency time with a white semi-transparent background.
          const displayText = timeToString(tStart - details.wakeupTs);
          const measured = ctx.measureText(displayText);
          if (latencyWidth >= measured.width + 2) {
            ctx.fillStyle = 'rgba(255,255,255,0.7)';
            ctx.fillRect(
                wakeupPos + latencyWidth / 2 - measured.width / 2 - 1,
                MARGIN_TOP + RECT_HEIGHT - 12,
                measured.width + 2,
                11);
            ctx.textBaseline = 'bottom';
            ctx.fillStyle = 'black';
            ctx.fillText(
                displayText,
                wakeupPos + (latencyWidth) / 2,
                MARGIN_TOP + RECT_HEIGHT - 1);
          }
        }
      }

      // Draw diamond if the track being drawn is the cpu of the waker.
      if (this.config.cpu === details.wakerCpu && details.wakeupTs) {
        const wakeupPos = Math.floor(timeScale.timeToPx(details.wakeupTs));
        ctx.beginPath();
        ctx.moveTo(wakeupPos, MARGIN_TOP + RECT_HEIGHT / 2 + 8);
        ctx.fillStyle = 'black';
        ctx.lineTo(wakeupPos + 6, MARGIN_TOP + RECT_HEIGHT / 2);
        ctx.lineTo(wakeupPos, MARGIN_TOP + RECT_HEIGHT / 2 - 8);
        ctx.lineTo(wakeupPos - 6, MARGIN_TOP + RECT_HEIGHT / 2);
        ctx.fill();
        ctx.closePath();
      }
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
    const {timeScale} = globals.frontendLocalState;
    if (y < MARGIN_TOP || y > MARGIN_TOP + RECT_HEIGHT) {
      this.utidHoveredInThisTrack = -1;
      globals.frontendLocalState.setHoveredUtidAndPid(-1, -1);
      return;
    }
    const t = timeScale.pxToTime(x);
    let hoveredUtid = -1;

    for (let i = 0; i < data.starts.length; i++) {
      const tStart = data.starts[i];
      const tEnd = data.ends[i];
      const utid = data.utids[i];
      if (tStart <= t && t <= tEnd) {
        hoveredUtid = utid;
        break;
      }
    }
    this.utidHoveredInThisTrack = hoveredUtid;
    const threadInfo = globals.threads.get(hoveredUtid);
    const hoveredPid = threadInfo ? (threadInfo.pid ? threadInfo.pid : -1) : -1;
    globals.frontendLocalState.setHoveredUtidAndPid(hoveredUtid, hoveredPid);
  }

  onMouseOut() {
    this.utidHoveredInThisTrack = -1;
    globals.frontendLocalState.setHoveredUtidAndPid(-1, -1);
    this.mouseXpos = 0;
  }

  onMouseClick({x}: {x: number}) {
    const data = this.data();
    if (data === undefined || data.kind === 'summary') return false;
    const {timeScale} = globals.frontendLocalState;
    const time = timeScale.pxToTime(x);
    const index = search(data.starts, time);
    const id = index === -1 ? undefined : data.ids[index];
    if (!id || this.utidHoveredInThisTrack === -1) return false;
    globals.makeSelection(
        Actions.selectSlice({id, trackId: this.trackState.id}));
    return true;
  }
}

trackRegistry.register(CpuSliceTrack);
