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

import {Actions} from '../common/actions';
import {drawDoubleHeadedArrow} from '../common/canvas_utils';
import {translateState} from '../common/thread_state';
import {timeToCode, toNs} from '../common/time';

import {globals, SliceDetails, ThreadDesc} from './globals';
import {Panel, PanelSize} from './panel';
import {scrollToTrackAndTs} from './scroll_helper';

export class SliceDetailsPanel extends Panel {
  view() {
    const sliceInfo = globals.sliceDetails;
    if (sliceInfo.utid === undefined) return;
    const threadInfo = globals.threads.get(sliceInfo.utid);

    return m(
        '.details-panel',
        m('.details-panel-heading',
          m('h2.split', `Slice Details`),
          (sliceInfo.wakeupTs && sliceInfo.wakerUtid) ?
              m('h2.split', 'Scheduling Latency') :
              ''),
        this.getDetails(sliceInfo, threadInfo));
  }

  getDetails(sliceInfo: SliceDetails, threadInfo: ThreadDesc|undefined) {
    if (!threadInfo || sliceInfo.ts === undefined ||
        sliceInfo.dur === undefined) {
      return null;
    } else {
      return m(
          '.details-table',
          m('table.half-width',
            [
              m('tr',
                m('th', `Process`),
                m('td', `${threadInfo.procName} [${threadInfo.pid}]`)),
              m('tr',
                m('th', `Thread`),
                m('td',
                  `${threadInfo.threadName} [${threadInfo.tid}]`,
                  m('i.material-icons.grey',
                    {onclick: () => this.goToThread(), title: 'Go to thread'},
                    'call_made'))),
              m('tr',
                m('th', `Start time`),
                m('td', `${timeToCode(sliceInfo.ts)}`)),
              m('tr',
                m('th', `Duration`),
                m('td', `${timeToCode(sliceInfo.dur)}`)),
              m('tr', m('th', `Prio`), m('td', `${sliceInfo.priority}`)),
              m('tr',
                m('th', `End State`),
                m('td', `${translateState(sliceInfo.endState)}`))
            ]),
      );
    }
  }

  goToThread() {
    const sliceInfo = globals.sliceDetails;
    if (sliceInfo.utid === undefined) return;
    const threadInfo = globals.threads.get(sliceInfo.utid);

    if (sliceInfo.id === undefined || sliceInfo.ts === undefined ||
        sliceInfo.dur === undefined || sliceInfo.cpu === undefined ||
        threadInfo === undefined) {
      return;
    }

    let trackId: string|number|undefined;
    for (const track of Object.values(globals.state.tracks)) {
      if (track.kind === 'ThreadStateTrack' &&
          (track.config as {utid: number}).utid === threadInfo.utid) {
        trackId = track.id;
      }
    }

    if (trackId) {
      globals.makeSelection(Actions.selectThreadState({
        utid: threadInfo.utid,
        ts: sliceInfo.ts + globals.state.traceTime.startSec,
        dur: sliceInfo.dur,
        state: 'Running',
        cpu: sliceInfo.cpu,
        trackId: trackId.toString(),
      }));

      scrollToTrackAndTs(
          trackId, toNs(sliceInfo.ts + globals.state.traceTime.startSec), true);
    }
  }


  renderCanvas(ctx: CanvasRenderingContext2D, size: PanelSize) {
    const details = globals.sliceDetails;
    // Show expanded details on the scheduling of the currently selected slice.
    if (details.wakeupTs && details.wakerUtid !== undefined) {
      const threadInfo = globals.threads.get(details.wakerUtid);
      // Draw diamond and vertical line.
      const startDraw = {x: size.width / 2 + 20, y: 52};
      ctx.beginPath();
      ctx.moveTo(startDraw.x, startDraw.y + 28);
      ctx.fillStyle = 'black';
      ctx.lineTo(startDraw.x + 6, startDraw.y + 20);
      ctx.lineTo(startDraw.x, startDraw.y + 12);
      ctx.lineTo(startDraw.x - 6, startDraw.y + 20);
      ctx.fill();
      ctx.closePath();
      ctx.fillRect(startDraw.x - 1, startDraw.y, 2, 100);

      // Wakeup explanation text.
      ctx.font = '13px Roboto Condensed';
      ctx.fillStyle = '#3c4b5d';
      if (threadInfo) {
        const displayText = `Wakeup @ ${
            timeToCode(
                details.wakeupTs - globals.state.traceTime.startSec)} on CPU ${
            details.wakerCpu} by`;
        const processText = `P: ${threadInfo.procName} [${threadInfo.pid}]`;
        const threadText = `T: ${threadInfo.threadName} [${threadInfo.tid}]`;
        ctx.fillText(displayText, startDraw.x + 20, startDraw.y + 20);
        ctx.fillText(processText, startDraw.x + 20, startDraw.y + 37);
        ctx.fillText(threadText, startDraw.x + 20, startDraw.y + 55);
      }

      // Draw latency arrow and explanation text.
      drawDoubleHeadedArrow(ctx, startDraw.x, startDraw.y + 80, 60, true);
      if (details.ts) {
        const displayLatency = `Scheduling latency: ${
            timeToCode(
                details.ts -
                (details.wakeupTs - globals.state.traceTime.startSec))}`;
        ctx.fillText(displayLatency, startDraw.x + 70, startDraw.y + 86);
        const explain1 =
            'This is the interval from when the task became eligible to run';
        const explain2 =
            '(e.g. because of notifying a wait queue it was suspended on) to';
        const explain3 = 'when it started running.';
        ctx.font = '10px Roboto Condensed';
        ctx.fillText(explain1, startDraw.x + 70, startDraw.y + 86 + 16);
        ctx.fillText(explain2, startDraw.x + 70, startDraw.y + 86 + 16 + 12);
        ctx.fillText(explain3, startDraw.x + 70, startDraw.y + 86 + 16 + 24);
      }
    }
  }
}
