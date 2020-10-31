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

import {search} from '../../base/binary_search';
import {assertTrue} from '../../base/logging';
import {TrackState} from '../../common/state';
import {checkerboardExcept} from '../../frontend/checkerboard';
import {hueForCpu} from '../../frontend/colorizer';
import {globals} from '../../frontend/globals';
import {Track} from '../../frontend/track';
import {trackRegistry} from '../../frontend/track_registry';

import {
  Config,
  CPU_FREQ_TRACK_KIND,
  Data,
} from './common';

// 0.5 Makes the horizontal lines sharp.
const MARGIN_TOP = 4.5;
const RECT_HEIGHT = 20;

class CpuFreqTrack extends Track<Config, Data> {
  static readonly kind = CPU_FREQ_TRACK_KIND;
  static create(trackState: TrackState): CpuFreqTrack {
    return new CpuFreqTrack(trackState);
  }

  private mouseXpos = 0;
  private hoveredValue: number|undefined = undefined;
  private hoveredTs: number|undefined = undefined;
  private hoveredTsEnd: number|undefined = undefined;
  private hoveredIdle: number|undefined = undefined;

  constructor(trackState: TrackState) {
    super(trackState);
  }

  getHeight() {
    return MARGIN_TOP + RECT_HEIGHT;
  }

  renderCanvas(ctx: CanvasRenderingContext2D): void {
    // TODO: fonts and colors should come from the CSS and not hardcoded here.
    const {timeScale, visibleWindowTime} = globals.frontendLocalState;
    const data = this.data();

    if (data === undefined) return;  // Can't possibly draw anything.

    assertTrue(data.tsStarts.length === data.freqKHz.length);
    assertTrue(data.freqKHz.length === data.idles.length);

    const startPx = Math.floor(timeScale.timeToPx(visibleWindowTime.start));
    const endPx = Math.floor(timeScale.timeToPx(visibleWindowTime.end));
    const zeroY = MARGIN_TOP + RECT_HEIGHT;

    let lastX = startPx;
    let lastY = zeroY;

    // Quantize the Y axis to quarters of powers of tens (7.5K, 10K, 12.5K).
    let yMax = data.maximumValue;
    const kUnits = ['', 'K', 'M', 'G', 'T', 'E'];
    const exp = Math.ceil(Math.log10(Math.max(yMax, 1)));
    const pow10 = Math.pow(10, exp);
    yMax = Math.ceil(yMax / (pow10 / 4)) * (pow10 / 4);
    const unitGroup = Math.floor(exp / 3);
    const num = yMax / Math.pow(10, unitGroup * 3);
    // The values we have for cpufreq are in kHz so +1 to unitGroup.
    const yLabel = `${num} ${kUnits[unitGroup + 1]}Hz`;

    // Draw the CPU frequency graph.
    const hue = hueForCpu(this.config.cpu);
    let saturation = 45;
    if (globals.frontendLocalState.hoveredUtid !== -1) {
      saturation = 0;
    }
    ctx.fillStyle = `hsl(${hue}, ${saturation}%, 70%)`;
    ctx.strokeStyle = `hsl(${hue}, ${saturation}%, 55%)`;
    ctx.beginPath();
    ctx.moveTo(lastX, lastY);

    for (let i = 0; i < data.freqKHz.length; i++) {
      const value = data.freqKHz[i];
      const startTime = data.tsStarts[i];
      const nextY = zeroY - Math.round((value / yMax) * RECT_HEIGHT);
      if (nextY === lastY) continue;

      lastX = Math.floor(timeScale.timeToPx(startTime));
      ctx.lineTo(lastX, lastY);
      ctx.lineTo(lastX, nextY);
      lastY = nextY;
    }
    // Find the end time for the last frequency event and then draw
    // down to zero to show that we do not have data after that point.
    const endTime = data.tsEnds[data.freqKHz.length - 1];
    const finalX = Math.floor(timeScale.timeToPx(endTime));
    ctx.lineTo(finalX, lastY);
    ctx.lineTo(finalX, zeroY);
    ctx.lineTo(endPx, zeroY);
    ctx.closePath();
    ctx.fill();
    ctx.stroke();

    // Draw CPU idle rectangles that overlay the CPU freq graph.
    ctx.fillStyle = `rgba(240, 240, 240, 1)`;
    const bottomY = MARGIN_TOP + RECT_HEIGHT;

    for (let i = 0; i < data.freqKHz.length; i++) {
      if (data.idles[i] >= 0) {
        const value = data.freqKHz[i];
        const firstX = Math.floor(timeScale.timeToPx(data.tsStarts[i]));
        const secondX = Math.floor(timeScale.timeToPx(data.tsEnds[i]));
        const lastY = zeroY - Math.round((value / yMax) * RECT_HEIGHT);
        ctx.fillRect(firstX, bottomY, secondX - firstX, lastY - bottomY);
      }
    }

    ctx.font = '10px Roboto Condensed';

    if (this.hoveredValue !== undefined && this.hoveredTs !== undefined) {
      let text = `${this.hoveredValue.toLocaleString()}kHz`;
      if (data.isQuantized) {
        text = `${this.hoveredValue.toLocaleString()}kHz (weighted avg)`;
      }

      ctx.fillStyle = `hsl(${hue}, 45%, 75%)`;
      ctx.strokeStyle = `hsl(${hue}, 45%, 45%)`;

      const xStart = Math.floor(timeScale.timeToPx(this.hoveredTs));
      const xEnd = this.hoveredTsEnd === undefined ?
          endPx :
          Math.floor(timeScale.timeToPx(this.hoveredTsEnd));
      const y = zeroY - Math.round((this.hoveredValue / yMax) * RECT_HEIGHT);

      // Highlight line.
      ctx.beginPath();
      ctx.moveTo(xStart, y);
      ctx.lineTo(xEnd, y);
      ctx.lineWidth = 3;
      ctx.stroke();
      ctx.lineWidth = 1;

      // Draw change marker.
      ctx.beginPath();
      ctx.arc(xStart, y, 3 /*r*/, 0 /*start angle*/, 2 * Math.PI /*end angle*/);
      ctx.fill();
      ctx.stroke();

      // Display idle value if current hover is idle.
      if (this.hoveredIdle !== undefined && this.hoveredIdle !== -1) {
        // Display the idle value +1 to be consistent with catapult.
        text += ` (Idle: ${(this.hoveredIdle + 1).toLocaleString()})`;
      }

      // Draw the tooltip.
      this.drawTrackHoverTooltip(ctx, this.mouseXpos, text);
    }

    // Write the Y scale on the top left corner.
    ctx.textBaseline = 'alphabetic';
    ctx.fillStyle = 'rgba(255, 255, 255, 0.6)';
    ctx.fillRect(0, 0, 42, 18);
    ctx.fillStyle = '#666';
    ctx.textAlign = 'left';
    ctx.fillText(`${yLabel}`, 4, 14);

    // If the cached trace slices don't fully cover the visible time range,
    // show a gray rectangle with a "Loading..." label.
    checkerboardExcept(
        ctx,
        this.getHeight(),
        timeScale.timeToPx(visibleWindowTime.start),
        timeScale.timeToPx(visibleWindowTime.end),
        timeScale.timeToPx(data.start),
        timeScale.timeToPx(data.end));
  }

  onMouseMove({x}: {x: number, y: number}) {
    const data = this.data();
    if (data === undefined) return;
    this.mouseXpos = x;
    const {timeScale} = globals.frontendLocalState;
    const time = timeScale.pxToTime(x);

    const index = search(data.tsStarts, time);
    this.hoveredTs = index === -1 ? undefined : data.tsStarts[index];
    this.hoveredTsEnd = index === -1 ? undefined : data.tsEnds[index];
    this.hoveredValue = index === -1 ? undefined : data.freqKHz[index];
    this.hoveredIdle = index === -1 ? undefined : data.idles[index];
  }

  onMouseOut() {
    this.hoveredValue = undefined;
    this.hoveredTs = undefined;
    this.hoveredTsEnd = undefined;
    this.hoveredIdle = undefined;
  }
}

trackRegistry.register(CpuFreqTrack);
