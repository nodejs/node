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

import {showModal} from './modal';

// Never show more than one dialog per minute.
const MIN_REPORT_PERIOD_MS = 60000;
let timeLastReport = 0;

// Keeps the last ERR_QUEUE_MAX_LEN errors while the dialog is throttled.
const queuedErrors = new Array<string>();
const ERR_QUEUE_MAX_LEN = 10;

export function maybeShowErrorDialog(errLog: string) {
  const now = performance.now();
  if (timeLastReport > 0 && now - timeLastReport <= MIN_REPORT_PERIOD_MS) {
    queuedErrors.unshift(errLog);
    if (queuedErrors.length > ERR_QUEUE_MAX_LEN) queuedErrors.pop();
    console.log('Suppressing crash dialog, last error notified too soon.');
    return;
  }
  timeLastReport = now;

  // Append queued errors.
  while (queuedErrors.length > 0) {
    const queuedErr = queuedErrors.shift();
    errLog += `\n\n---------------------------------------\n${queuedErr}`;
  }

  const errTitle = errLog.split('\n', 1)[0].substr(0, 80);
  let link = 'https://goto.google.com/perfetto-ui-bug';
  link += '?title=' + encodeURIComponent(`UI Error: ${errTitle}`);
  link += '&description=' + encodeURIComponent(errLog.substr(0, 32768));

  showModal({
    title: 'Oops, something went wrong. Please file a bug',
    content: m(
        'div',
        m('span', 'An unexpected crash occurred:'),
        m('.modal-logs', errLog),
        ),
    buttons: [
      {
        text: 'File a bug (Googlers only)',
        primary: true,
        id: 'file_bug',
        action: () => {
          window.open(link, '_blank');
        }
      },
    ]
  });
}
