// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {Profile} from '../../profile.mjs'

import {LogEntry} from './log.mjs';

export class TickLogEntry extends LogEntry {
  constructor(time, vmState, processedStack) {
    super(TickLogEntry.extractType(vmState, processedStack), time);
    /** @type {string} */
    this.state = vmState;
    /** @type {CodeEntry[]} */
    this.stack = processedStack;
    /** @type {number} */
    this._endTime = time;
  }

  end(time) {
    if (this.isInitialized) throw new Error('Invalid timer change');
    this._endTime = time;
  }

  get isInitialized() {
    return this._endTime !== this._time;
  }

  get startTime() {
    return this._time;
  }

  get endTime() {
    return this._endTime;
  }

  get duration() {
    return this._endTime - this._time;
  }

  static extractType(vmState, processedStack) {
    if (processedStack.length == 0 || vmState == Profile.VMState.IDLE) {
      return 'Idle';
    }
    const topOfStack = processedStack[0];
    if (typeof topOfStack === 'number') {
      // TODO(cbruni): Handle VmStack and native ticks better.
      return 'Other';
    }
    if (vmState != Profile.VMState.JS) {
      topOfStack.vmState = vmState;
    }
    return this.extractCodeEntryType(topOfStack);
  }

  static extractCodeEntryType(entry) {
    if (entry?.state !== undefined) {
      return 'JS ' + Profile.getKindFromState(entry.state);
    }
    if (entry?.vmState) return Profile.vmStateString(entry.vmState);
    return 'Other';
  }
}
