// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
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

  get functionName() {
    return this._entry.functionName;
  }

  toString() {
    return `Deopt(${this.type})`;
  }

  toStringLong() {
    return `Deopt(${this.type})${this._reason}: ${this._location}`;
  }

  static get propertyNames() {
    return [
      'type', 'reason', 'functionName', 'sourcePosition',
      'functionSourcePosition', 'script'
    ];
  }
}

export class CodeLogEntry extends LogEntry {
  constructor(type, time, kind, entry) {
    super(type, time);
    this._kind = kind;
    this._entry = entry;
  }

  get kind() {
    return this._kind;
  }

  get entry() {
    return this._entry;
  }

  get functionName() {
    return this._entry.functionName;
  }

  get size() {
    return this._entry.size;
  }

  get source() {
    return this._entry?.getSourceCode() ?? '';
  }

  get disassemble() {
    return this._entry?.source?.disassemble;
  }

  toString() {
    return `Code(${this.type})`;
  }

  toStringLong() {
    return `Code(${this.type}): ${this._entry.toString()}`;
  }

  static get propertyNames() {
    return ['type', 'kind', 'functionName', 'sourcePosition', 'script'];
  }
}
