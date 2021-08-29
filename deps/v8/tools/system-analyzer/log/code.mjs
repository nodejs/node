// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {formatBytes} from '../helper.mjs';

import {LogEntry} from './log.mjs';

export class DeoptLogEntry extends LogEntry {
  constructor(
      type, time, entry, deoptReason, deoptLocation, scriptOffset,
      instructionStart, codeSize, inliningId) {
    super(type, time);
    this._entry = entry;
    this._reason = deoptReason;
    this._location = deoptLocation;
    this._scriptOffset = scriptOffset;
    this._instructionStart = instructionStart;
    this._codeSize = codeSize;
    this._inliningId = inliningId;
    this.fileSourcePosition = undefined;
  }

  get reason() {
    return this._reason;
  }

  get location() {
    return this._location;
  }

  get entry() {
    return this._entry;
  }

  get code() {
    return this._entry?.logEntry;
  }

  get functionName() {
    return this._entry.functionName;
  }

  static get propertyNames() {
    return [
      'type', 'reason', 'functionName', 'sourcePosition',
      'functionSourcePosition', 'script', 'code'
    ];
  }
}

export class CodeLogEntry extends LogEntry {
  constructor(type, time, kindName, kind, entry) {
    super(type, time);
    this._kind = kind;
    this._kindName = kindName;
    this._entry = entry;
    entry.logEntry = this;
  }

  get kind() {
    return this._kind;
  }

  get isBuiltinKind() {
    return this._kindName === 'Builtin';
  }

  get kindName() {
    return this._kindName;
  }

  get entry() {
    return this._entry;
  }

  get functionName() {
    return this._entry.functionName ?? this._entry.getRawName();
  }

  get size() {
    return this._entry.size;
  }

  get source() {
    return this._entry?.getSourceCode() ?? '';
  }

  get code() {
    return this._entry?.source?.disassemble;
  }

  get variants() {
    const entries = Array.from(this.entry?.func?.codeEntries ?? []);
    return entries.map(each => each.logEntry);
  }

  toString() {
    return `Code(${this.type})`;
  }

  get toolTipDict() {
    const dict = super.toolTipDict;
    dict.size = formatBytes(dict.size);
    return dict;
  }

  static get propertyNames() {
    return [
      'functionName', 'sourcePosition', 'kindName', 'size', 'type', 'kind',
      'script', 'source', 'code', 'variants'
    ];
  }
}

export class SharedLibLogEntry extends LogEntry {
  constructor(entry) {
    super('SHARED_LIB', 0);
    this._entry = entry;
  }

  get name() {
    return this._entry.name;
  }

  get entry() {
    return this._entry;
  }

  toString() {
    return `SharedLib`;
  }

  static get propertyNames() {
    return ['name'];
  }
}
