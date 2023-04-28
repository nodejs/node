// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {formatDurationMicros} from '../helper.mjs';

import {LogEntry} from './log.mjs';

export class TimerLogEntry extends LogEntry {
  constructor(type, startTime, endTime = -1) {
    super(type, startTime);
    this._endTime = endTime;
    this.depth = 0;
  }

  end(time) {
    if (this.isInitialized) throw new Error('Invalid timer change');
    this._endTime = time;
  }

  get isInitialized() {
    return this._endTime !== -1;
  }

  get startTime() {
    return this._time;
  }

  get endTime() {
    return this._endTime;
  }

  get duration() {
    return Math.max(0, this._endTime - this._time);
  }

  covers(time) {
    return this._time <= time && time <= this._endTime;
  }

  get toolTipDict() {
    const dict = super.toolTipDict;
    dict.startTime = formatDurationMicros(dict.startTime);
    dict.endTime = formatDurationMicros(dict.endTime);
    dict.duration = formatDurationMicros(dict.duration);
    return dict;
  }

  static get propertyNames() {
    return [
      'type',
      'startTime',
      'endTime',
      'duration',
    ];
  }
}
