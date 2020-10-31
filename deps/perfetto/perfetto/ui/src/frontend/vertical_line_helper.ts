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

import {TRACK_SHELL_WIDTH} from './css_constants';
import {TimeScale} from './time_scale';

export function drawVerticalLineAtTime(ctx: CanvasRenderingContext2D,
                                       timeScale: TimeScale,
                                       time: number,
                                       height: number,
                                       color: string,
                                       lineWidth = 2) {
    const xPos = TRACK_SHELL_WIDTH + Math.floor(timeScale.timeToPx(time));
    drawVerticalLine(ctx, xPos, height, color, lineWidth);
  }

function drawVerticalLine(ctx: CanvasRenderingContext2D,
                          xPos: number,
                          height: number,
                          color: string,
                          lineWidth = 2) {
    ctx.beginPath();
    ctx.strokeStyle = color;
    const prevLineWidth = ctx.lineWidth;
    ctx.lineWidth = lineWidth;
    ctx.moveTo(xPos, 0);
    ctx.lineTo(xPos, height);
    ctx.stroke();
    ctx.closePath();
    ctx.lineWidth = prevLineWidth;
}

// This draws two shaded rectangles outside of the area of interest. Effectivly
// highlighting an area by colouring/darkening the outside areas.
export function drawVerticalSelection(
    ctx: CanvasRenderingContext2D,
    timeScale: TimeScale,
    timeStart: number,
    timeEnd: number,
    height: number,
    color: string) {
  const xStartPos =
      TRACK_SHELL_WIDTH + Math.floor(timeScale.timeToPx(timeStart));
  const xEndPos = TRACK_SHELL_WIDTH + Math.floor(timeScale.timeToPx(timeEnd));
  const width = timeScale.endPx;
  ctx.fillStyle = color;
  ctx.fillRect(0, 0, xStartPos, height);
  // In the worst case xEndPos may be far to the left of the canvas (and so be
  // <0) in this case fill the whole screen.
  ctx.fillRect(Math.max(xEndPos, 0), 0, width + TRACK_SHELL_WIDTH, height);
  drawVerticalLine(ctx, xStartPos, height, `rgba(52,69,150)`);
  drawVerticalLine(ctx, xEndPos, height, `rgba(52,69,150)`);
}
