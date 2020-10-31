
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

import * as m from 'mithril';

import {hideModel, showModal} from './modal';

let helpModelOpen = false;

export function toggleHelp() {
  if (helpModelOpen) {
    hideHelp();
  } else {
    showHelp();
  }
}

function keycap(key: string) {
  return m('.keycap', key);
}

function showHelp() {
  helpModelOpen = true;
  showModal({
    title: 'Perfetto Help',
    content: m(
        '.help',
        m('h2', 'Navigation'),
        m(
            'table',
            m(
                'tr',
                m('td', keycap('w'), '/', keycap('s')),
                m('td', 'Zoom in/out'),
                ),
            m(
                'tr',
                m('td', keycap('a'), '/', keycap('d')),
                m('td', 'Pan left/right'),
                ),
            ),
        m('h2', 'Mouse Controls'),
        m('table',
          m('tr', m('td', 'Click'), m('td', 'Select event')),
          m('tr', m('td', 'Ctrl + Scroll wheel'), m('td', 'Zoom in/out')),
          m('tr', m('td', 'Click + Drag'), m('td', 'Select area')),
          m('tr', m('td', 'Shift + Click + Drag'), m('td', 'Pan left/right'))),
        m('h2', 'Other'),
        m(
            'table',
            m('tr',
              m('td', keycap('f'), ' (with event selected)'),
              m('td', 'Scroll + zoom to current selection')),
            m('tr',
              m('td', keycap('m'), ' (with event or area selected)'),
              m('td', 'Mark the area')),
            m('tr', m('td', keycap('?')), m('td', 'Show help')),
            )),
    buttons: [],
  }).finally(() => {
    helpModelOpen = false;
  });
}

function hideHelp() {
  if (helpModelOpen) {
    hideModel();
  }
}
