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

import {TRACK_BORDER_COLOR, TRACK_SHELL_WIDTH} from './css_constants';
import {TimeScale} from './time_scale';

export const DESIRED_PX_PER_STEP = 80;

/**
 * Returns the step size of a grid line in seconds.
 * The returned step size has two properties:
 * (1) It is 1, 2, or 5, multiplied by some integer power of 10.
 * (2) The number steps in |range| produced by |stepSize| is as close as
 *     possible to |desiredSteps|.
 */
export function getGridStepSize(range: number, desiredSteps: number): number {
  // First, get the largest possible power of 10 that is smaller than the
  // desired step size, and set it to the current step size.
  // For example, if the range is 2345ms and the desired steps is 10, then the
  // desired step size is 234.5 and the step size will be set to 100.
  const desiredStepSize = range / desiredSteps;
  const zeros = Math.floor(Math.log10(desiredStepSize));
  const initialStepSize = Math.pow(10, zeros);

  // This function first calculates how many steps within the range a certain
  // stepSize will produce, and returns the difference between that and
  // desiredSteps.
  const distToDesired = (evaluatedStepSize: number) =>
      Math.abs(range / evaluatedStepSize - desiredSteps);

  // We know that |initialStepSize| is a power of 10, and
  // initialStepSize <= desiredStepSize <= 10 * initialStepSize. There are four
  // possible candidates for final step size: 1, 2, 5 or 10 * initialStepSize.
  // We pick the candidate that minimizes distToDesired(stepSize).
  const stepSizeMultipliers = [2, 5, 10];

  let minimalDistance = distToDesired(initialStepSize);
  let minimizingStepSize = initialStepSize;

  for (const multiplier of stepSizeMultipliers) {
    const newStepSize = multiplier * initialStepSize;
    const newDistance = distToDesired(newStepSize);
    if (newDistance < minimalDistance) {
      minimalDistance = newDistance;
      minimizingStepSize = newStepSize;
    }
  }
  return minimizingStepSize;
}

/**
 * Generator that returns that (given a width im px, span, and scale) returns
 * pairs of [xInPx, timestampInS] pairs describing where gridlines should be
 * drawn.
 */
export function gridlines(width: number, span: TimeSpan, timescale: TimeScale):
    Array<[number, number]> {
  const desiredSteps = width / DESIRED_PX_PER_STEP;
  const step = getGridStepSize(span.duration, desiredSteps);
  const actualSteps = Math.floor(span.duration / step);
  const start = Math.round(span.start / step) * step;
  const lines: Array<[number, number]> = [];
  let previousTimestamp = Number.NEGATIVE_INFINITY;
  // Iterating over the number of steps instead of
  // for (let s = start; s < span.end; s += step) because if start is very large
  // number and step very small, s will never reach end.
  for (let i = 0; i <= actualSteps; i++) {
    let xPos = TRACK_SHELL_WIDTH;
    const timestamp = start + i * step;
    xPos += Math.floor(timescale.timeToPx(timestamp));
    if (xPos < TRACK_SHELL_WIDTH) continue;
    if (xPos > width) break;
    if (Math.abs(timestamp - previousTimestamp) > Number.EPSILON) {
      previousTimestamp = timestamp;
      lines.push([xPos, timestamp]);
    }
  }
  return lines;
}

export function drawGridLines(
    ctx: CanvasRenderingContext2D,
    x: TimeScale,
    timeSpan: TimeSpan,
    width: number,
    height: number): void {
  ctx.strokeStyle = TRACK_BORDER_COLOR;
  ctx.lineWidth = 1;

  for (const xAndTime of gridlines(width, timeSpan, x)) {
    ctx.beginPath();
    ctx.moveTo(xAndTime[0] + 0.5, 0);
    ctx.lineTo(xAndTime[0] + 0.5, height);
    ctx.stroke();
  }
}
