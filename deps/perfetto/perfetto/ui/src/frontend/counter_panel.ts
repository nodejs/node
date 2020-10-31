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

import {fromNs, timeToCode} from '../common/time';

import {globals} from './globals';
import {Panel} from './panel';

interface CounterDetailsPanelAttrs {}

export class CounterDetailsPanel extends Panel<CounterDetailsPanelAttrs> {
  view() {
    const counterInfo = globals.counterDetails;
    if (counterInfo && counterInfo.startTime &&
        counterInfo.value !== undefined && counterInfo.delta !== undefined &&
        counterInfo.duration !== undefined) {
      return m(
          '.details-panel',
          m('.details-panel-heading', m('h2', `Counter Details`)),
          m(
              '.details-table',
              [m('table.half-width',
                 [
                   m('tr',
                     m('th', `Start time`),
                     m('td', `${timeToCode(counterInfo.startTime)}`)),
                   m('tr',
                     m('th', `Value`),
                     m('td', `${counterInfo.value.toLocaleString()}`)),
                   m('tr',
                     m('th', `Delta`),
                     m('td', `${counterInfo.delta.toLocaleString()}`)),
                   m('tr',
                     m('th', `Duration`),
                     m('td', `${timeToCode(fromNs(counterInfo.duration))}`)),
                 ])],
              ));
    } else {
      return m(
          '.details-panel',
          m('.details-panel-heading', m('h2', `Counter Details`)));
    }
  }

  renderCanvas() {}
}
