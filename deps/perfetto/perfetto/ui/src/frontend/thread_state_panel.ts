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
import {translateState} from '../common/thread_state';
import {timeToCode, toNs} from '../common/time';

import {globals} from './globals';
import {Panel, PanelSize} from './panel';
import {scrollToTrackAndTs} from './scroll_helper';

interface ThreadStateDetailsAttr {
  utid: number;
  ts: number;
  dur: number;
  state: string;
  cpu: number;
}

export class ThreadStatePanel extends Panel<ThreadStateDetailsAttr> {
  view({attrs}: m.CVnode<ThreadStateDetailsAttr>) {
    const threadInfo = globals.threads.get(attrs.utid);
    if (threadInfo) {
      return m(
          '.details-panel',
          m('.details-panel-heading', m('h2', 'Thread State')),
          m('.details-table', [m('table.half-width', [
              m('tr',
                m('th', `Start time`),
                m('td',
                  `${
                      timeToCode(
                          attrs.ts - globals.state.traceTime.startSec)}`)),
              m('tr',
                m('th', `Duration`),
                m(
                    'td',
                    `${timeToCode(attrs.dur)} `,
                    )),
              m('tr',
                m('th', `State`),
                m('td', this.getStateContent(attrs.state, attrs.cpu))),
              m('tr',
                m('th', `Process`),
                m('td', `${threadInfo.procName} [${threadInfo.pid}]`)),
            ])]));
    } else {
      return m('.details-panel');
    }
  }

  renderCanvas(_ctx: CanvasRenderingContext2D, _size: PanelSize) {}

  // If it is the running state, we want to show which CPU and a button to
  // go to the sched slice. Otherwise, just show the state.
  getStateContent(state: string, cpu: number) {
    if (state !== 'Running') {
      return [translateState(state)];
    }

    return [
      `${translateState(state)} on CPU ${cpu}`,
      m('i.material-icons.grey',
        {
          onclick: () => {
            if (globals.sliceDetails.id && globals.sliceDetails.ts) {
              // TODO(taylori): Use trackId from TP.
              let trackId;
              for (const track of Object.values(globals.state.tracks)) {
                if (track.kind === 'CpuSliceTrack' &&
                    (track.config as {cpu: number}).cpu === cpu) {
                  trackId = track.id;
                }
              }
              if (trackId) {
                globals.makeSelection(Actions.selectSlice(
                    {id: globals.sliceDetails.id, trackId}));
                scrollToTrackAndTs(
                    trackId,
                    toNs(
                        globals.sliceDetails.ts +
                        globals.state.traceTime.startSec));
              }
            }
          },
          title: 'Go to CPU slice'
        },
        'call_made')
    ];
  }
}
