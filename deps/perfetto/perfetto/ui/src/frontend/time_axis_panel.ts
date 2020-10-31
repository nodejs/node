// Copyright (C) 2018 The Android Open Source Project
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

import {timeToString} from '../common/time';

import {TRACK_SHELL_WIDTH} from './css_constants';
import {globals} from './globals';
import {gridlines} from './gridline_helper';
import {Panel, PanelSize} from './panel';

export class TimeAxisPanel extends Panel {
  view() {
    return m('.time-axis-panel');
  }

  renderCanvas(ctx: CanvasRenderingContext2D, size: PanelSize) {
    const timeScale = globals.frontendLocalState.timeScale;
    const range = globals.frontendLocalState.visibleWindowTime;
    ctx.fillStyle = '#999';

    // Write trace offset time + line.
    ctx.font = '12px Roboto Condensed';

    ctx.textAlign = 'right';
    const offsetTime =
        timeToString(range.start - globals.state.traceTime.startSec);
    ctx.fillText(offsetTime, TRACK_SHELL_WIDTH - 6, 11);

    ctx.textAlign = 'left';
    const startTime = timeToString(globals.state.traceTime.startSec);
    ctx.fillText(startTime + ' +', 6, 11);

    // Draw time axis.
    ctx.font = '10px Roboto Condensed';
    for (const [x, time] of gridlines(size.width, range, timeScale)) {
      ctx.fillRect(x, 0, 1, size.height);
      ctx.fillText('+' + timeToString(time - range.start), x + 5, 10);
    }

    ctx.fillRect(TRACK_SHELL_WIDTH - 2, 0, 2, size.height);
  }
}
