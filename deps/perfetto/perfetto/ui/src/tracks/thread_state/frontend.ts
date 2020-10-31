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

import {search, searchEq} from '../../base/binary_search';
import {Actions} from '../../common/actions';
import {cropText} from '../../common/canvas_utils';
import {TrackState} from '../../common/state';
import {translateState} from '../../common/thread_state';
import {checkerboardExcept} from '../../frontend/checkerboard';
import {Color, colorForState} from '../../frontend/colorizer';
import {globals} from '../../frontend/globals';
import {Track} from '../../frontend/track';
import {trackRegistry} from '../../frontend/track_registry';

import {
  Config,
  Data,
  StatePercent,
  THREAD_STATE_TRACK_KIND,
} from './common';

const MARGIN_TOP = 4;
const RECT_HEIGHT = 14;
const EXCESS_WIDTH = 10;

function groupBusyStates(resolution: number) {
  return resolution >= 0.0001;
}

function getSummarizedSliceText(breakdownMap: StatePercent) {
  let result = 'Various (';
  const sorted =
      new Map([...breakdownMap.entries()].sort((a, b) => b[1] - a[1]));
  for (const [state, value] of sorted.entries()) {
    result += `${state}: ${Math.round(value * 100)}%, `;
  }
  return result.slice(0, result.length - 2) + ')';
}

class ThreadStateTrack extends Track<Config, Data> {
  static readonly kind = THREAD_STATE_TRACK_KIND;
  static create(trackState: TrackState): ThreadStateTrack {
    return new ThreadStateTrack(trackState);
  }

  constructor(trackState: TrackState) {
    super(trackState);
  }

  getHeight(): number {
    return 2 * MARGIN_TOP + RECT_HEIGHT;
  }

  renderCanvas(ctx: CanvasRenderingContext2D): void {
    const {timeScale, visibleWindowTime} = globals.frontendLocalState;
    const data = this.data();
    const charWidth = ctx.measureText('dbpqaouk').width / 8;

    if (data === undefined) return;  // Can't possibly draw anything.

    checkerboardExcept(
        ctx,
        this.getHeight(),
        timeScale.timeToPx(visibleWindowTime.start),
        timeScale.timeToPx(visibleWindowTime.end),
        timeScale.timeToPx(data.start),
        timeScale.timeToPx(data.end),
    );

    const shouldGroupBusyStates = groupBusyStates(data.resolution);

    ctx.textAlign = 'center';
    ctx.font = '10px Roboto Condensed';

    for (let i = 0; i < data.starts.length; i++) {
      const tStart = data.starts[i];
      const tEnd = data.ends[i];
      const state = data.strings[data.state[i]];
      if (tEnd <= visibleWindowTime.start || tStart >= visibleWindowTime.end) {
        continue;
      }
      if (tStart && tEnd) {
        // Don't display a slice for Task Dead.
        if (state === 'x') continue;
        const rectStart = timeScale.timeToPx(tStart);
        const rectEnd = timeScale.timeToPx(tEnd);
        let rectWidth = rectEnd - rectStart;
        const color = colorForState(state);
        let text = translateState(state);
        const breakdown = data.summarisedStateBreakdowns.get(i);
        if (breakdown) {
          colorSummarizedSlice(breakdown, rectStart, rectEnd);
          text = getSummarizedSliceText(breakdown);
        } else {
          ctx.fillStyle = `hsl(${color.h},${color.s}%,${color.l}%)`;
        }
        if (shouldGroupBusyStates && rectWidth < 1) {
          rectWidth = 1;
        }
        ctx.fillRect(rectStart, MARGIN_TOP, rectWidth, RECT_HEIGHT);

        // Don't render text when we have less than 10px to play with.
        if (rectWidth < 10) continue;
        const title = cropText(text, charWidth, rectWidth);
        const rectXCenter = rectStart + rectWidth / 2;
        ctx.fillStyle = color.l > 80 || breakdown ? '#404040' : '#fff';
        ctx.fillText(title, rectXCenter, MARGIN_TOP + RECT_HEIGHT / 2 + 3);
      }
    }

    const selection = globals.state.currentSelection;
    if (selection !== null && selection.kind === 'THREAD_STATE' &&
        selection.utid === this.config.utid) {
      const [startIndex, endIndex] = searchEq(data.starts, selection.ts);
      if (startIndex !== endIndex) {
        const tStart = data.starts[startIndex];
        const tEnd = data.ends[startIndex];
        const state = data.strings[data.state[startIndex]];

        // If we try to draw too far off the end of the canvas (+/-4m~),
        // the line is not drawn. Instead limit drawing to the canvas
        // boundaries, but allow some excess to ensure that the start and end
        // of the rect are not shown unless that is truly when it starts/ends.
        const rectStart =
            Math.max(0 - EXCESS_WIDTH, timeScale.timeToPx(tStart));
        const rectEnd = Math.min(
            timeScale.timeToPx(visibleWindowTime.end) + EXCESS_WIDTH,
            timeScale.timeToPx(tEnd));
        const color = colorForState(state);
        ctx.strokeStyle = `hsl(${color.h},${color.s}%,${color.l * 0.7}%)`;
        ctx.beginPath();
        ctx.lineWidth = 3;
        ctx.strokeRect(
            rectStart, MARGIN_TOP - 1.5, rectEnd - rectStart, RECT_HEIGHT + 3);
        ctx.closePath();
      }
    }

    // Make a gradient ordered most common to least based on the colors of the
    // states within the summarized slice.
    function colorSummarizedSlice(
        breakdownMap: StatePercent, rectStart: number, rectEnd: number) {
      const gradient =
          ctx.createLinearGradient(rectStart, MARGIN_TOP, rectEnd, MARGIN_TOP);
      // Sometimes multiple states have the same color e.g R, R+
      const colorMap = new Map<Color, number>();
      for (const [state, value] of breakdownMap.entries()) {
        const color = colorForState(state);
        const currentColorValue = colorMap.get(color);
        if (currentColorValue === undefined) {
          colorMap.set(color, value);
        } else {
          colorMap.set(color, currentColorValue + value);
        }
      }

      const sorted =
          new Map([...colorMap.entries()].sort((a, b) => b[1] - a[1]));
      let colorStop = 0;
      for (const [color, value] of sorted.entries()) {
        const colorString = `hsl(${color.h},${color.s}%,${color.l}%)`;
        colorStop = Math.max(0, Math.min(1, colorStop + value));
        gradient.addColorStop(colorStop, colorString);
      }
      ctx.fillStyle = gradient;
    }
  }

  onMouseClick({x}: {x: number}) {
    const data = this.data();
    if (data === undefined) return false;
    const {timeScale} = globals.frontendLocalState;
    const time = timeScale.pxToTime(x);
    const index = search(data.starts, time);
    const ts = index === -1 ? undefined : data.starts[index];
    const tsEnd = index === -1 ? undefined : data.ends[index];
    const state = index === -1 ? undefined : data.strings[data.state[index]];
    const cpu = index === -1 ? undefined : data.cpu[index];
    const utid = this.config.utid;
    if (ts && state && tsEnd && cpu !== undefined) {
      globals.makeSelection(Actions.selectThreadState({
        utid,
        ts,
        dur: tsEnd - ts,
        state,
        cpu,
        trackId: this.trackState.id
      }));
      return true;
    }
    return false;
  }
}

trackRegistry.register(ThreadStateTrack);
