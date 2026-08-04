// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {LogEntry} from './log.mjs';

export class IcLogEntry extends LogEntry {
  constructor(
      type, fn_file, time, line, column, key, oldState, newState, map, reason,
      modifier, codeEntry) {
    super(type, time);
    this.category = 'other';
    if (this.type.indexOf('Store') !== -1) {
      this.category = 'Store';
    } else if (this.type.indexOf('Load') !== -1) {
      this.category = 'Load';
    }
    const parts = fn_file.split(' ');
    this.functionName = parts[0];
    this.file = parts[1];
    this.oldState = oldState;
    this.newState = newState;
    this.key = key;
    this.map = map;
    this.reason = reason;
    this.modifier = modifier;
    this.codeEntry = codeEntry;
  }

  get state() {
    return this.oldState + ' → ' + this.newState;
  }

  get code() {
    return this.codeEntry?.logEntry;
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

  static get propertyNames() {
    return [
      'type', 'category', 'functionName', 'script', 'sourcePosition', 'code',
      'state', 'key', 'map', 'reason'
    ];
  }
}
