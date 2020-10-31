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

import {timeToCode, toNs} from '../common/time';

import {globals} from './globals';
import {Panel, PanelSize} from './panel';

export class ChromeSliceDetailsPanel extends Panel {
  view() {
    const sliceInfo = globals.sliceDetails;
    if (sliceInfo.ts !== undefined && sliceInfo.dur !== undefined &&
        sliceInfo.name !== undefined) {
      return m(
          '.details-panel',
          m('.details-panel-heading', m('h2', `Slice Details`)),
          m(
              '.details-table',
              m('table.half-width',
                m('tr', m('th', `Name`), m('td', `${sliceInfo.name}`)),
                m('tr',
                  m('th', `Category`),
                  m('td',
                    `${
                        sliceInfo.category === '[NULL]' ?
                            'N/A' :
                            sliceInfo.category}`)),
                m('tr',
                  m('th', `Start time`),
                  m('td', `${timeToCode(sliceInfo.ts)}`)),
                m('tr',
                  m('th', `Duration`),
                  m('td',
                    `${
                        toNs(sliceInfo.dur) === -1 ?
                            '-1 (Did not end)' :
                            timeToCode(sliceInfo.dur)}`)),
                this.getDescription(sliceInfo.description)),
              this.getArgs(sliceInfo.args),
              ));
    } else {
      return m(
          '.details-panel',
          m('.details-panel-heading',
            m(
                'h2',
                `Slice Details`,
                )));
    }
  }

  renderCanvas(_ctx: CanvasRenderingContext2D, _size: PanelSize) {}

  getArgs(args?: Map<string, string>): m.Vnode[] {
    if (!args || args.size === 0) return [];
    const result = [m('tr', m('th', 'Args'))];
    for (const [key, value] of args) {
      result.push(m('tr', m('th', key), m('td', value)));
    }
    return result;
  }

  getDescription(description?: Map<string, string>): m.Vnode[] {
    if (!description) return [];
    const result = [];
    for (const [key, value] of description) {
      result.push(m('tr', m('th', key), m('td', value)));
    }
    return result;
  }
}
