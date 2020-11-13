// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import { Event } from './log.mjs';

class IcLogEvent extends Event {
  constructor(
    type, fn_file, time, line, column, key, oldState, newState, map, reason,
    script, additional) {
    super(type, time);
    this.category = 'other';
    if (this.type.indexOf('Store') !== -1) {
      this.category = 'Store';
    } else if (this.type.indexOf('Load') !== -1) {
      this.category = 'Load';
    }
    let parts = fn_file.split(' ');
    this.functionName = parts[0];
    this.file = parts[1];
    let position = line + ':' + column;
    this.filePosition = this.file + ':' + position;
    this.oldState = oldState;
    this.newState = newState;
    this.state = this.oldState + ' → ' + this.newState;
    this.key = key;
    this.map = map;
    this.reason = reason;
    this.additional = additional;
    this.script = script;
  }


  parseMapProperties(parts, offset) {
    let next = parts[++offset];
    if (!next.startsWith('dict')) return offset;
    this.propertiesMode = next.substr(5) == '0' ? 'fast' : 'slow';
    this.numberOfOwnProperties = parts[++offset].substr(4);
    next = parts[++offset];
    this.instanceType = next.substr(5, next.length - 6);
    return offset;
  }

  parsePositionAndFile(parts, start) {
    // find the position of 'at' in the parts array.
    let offset = start;
    for (let i = start + 1; i < parts.length; i++) {
      offset++;
      if (parts[i] == 'at') break;
    }
    if (parts[offset] !== 'at') return -1;
    this.position = parts.slice(start, offset).join(' ');
    offset += 1;
    this.isNative = parts[offset] == 'native'
    offset += this.isNative ? 1 : 0;
    this.file = parts[offset];
    return offset;
  }
}

export { IcLogEvent };
