// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use size file except in compliance with the License.
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
import {fromNs} from '../common/time';

import {TRACK_SHELL_WIDTH} from './css_constants';
import {globals} from './globals';
import {gridlines} from './gridline_helper';
import {Panel, PanelSize} from './panel';

// This is used to display the summary of search results.
export class TickmarkPanel extends Panel {
  view() {
    return m('.tickbar');
  }

  renderCanvas(ctx: CanvasRenderingContext2D, size: PanelSize) {
    const {timeScale, visibleWindowTime} = globals.frontendLocalState;

    ctx.fillStyle = '#999';
    ctx.fillRect(TRACK_SHELL_WIDTH - 2, 0, 2, size.height);
    for (const xAndTime of gridlines(
             size.width, visibleWindowTime, timeScale)) {
      ctx.fillRect(xAndTime[0], 0, 1, size.height);
    }

    const data = globals.searchSummary;
    for (let i = 0; i < data.tsStarts.length; i++) {
      const tStart = data.tsStarts[i];
      const tEnd = data.tsEnds[i];
      if (tEnd <= visibleWindowTime.start || tStart >= visibleWindowTime.end) {
        continue;
      }
      const rectStart =
          Math.max(timeScale.timeToPx(tStart), 0) + TRACK_SHELL_WIDTH;
      const rectEnd = timeScale.timeToPx(tEnd) + TRACK_SHELL_WIDTH;
      ctx.fillStyle = '#ffe263';
      ctx.fillRect(
          Math.floor(rectStart),
          0,
          Math.ceil(rectEnd - rectStart),
          size.height);
    }
    const index = globals.frontendLocalState.searchIndex;
    const startSec = fromNs(globals.currentSearchResults.tsStarts[index]);
    const triangleStart =
        Math.max(timeScale.timeToPx(startSec), 0) + TRACK_SHELL_WIDTH;
    ctx.fillStyle = '#000';
    ctx.beginPath();
    ctx.moveTo(triangleStart, size.height);
    ctx.lineTo(triangleStart - 3, 0);
    ctx.lineTo(triangleStart + 3, 0);
    ctx.lineTo(triangleStart, size.height);
    ctx.fill();
    ctx.closePath();
  }
}
