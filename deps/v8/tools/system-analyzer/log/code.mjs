// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {formatBytes} from '../helper.mjs';
import {LogEntry} from './log.mjs';

class CodeString {
  constructor(string) {
    if (typeof string !== 'string') {
      throw new Error('Expected string');
    }
    this.string = string;
  }

  get isCode() {
    return true;
  }

  toString() {
    return this.string;
  }
}

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

class CodeLikeLogEntry extends LogEntry {
  constructor(type, time, profilerEntry) {
    super(type, time);
    this._entry = profilerEntry;
    profilerEntry.logEntry = this;
    this._relatedEntries = [];
  }

  get entry() {
    return this._entry;
  }

  add(entry) {
    this._relatedEntries.push(entry);
  }

  relatedEntries() {
    return this._relatedEntries;
  }
}

export class CodeLogEntry extends CodeLikeLogEntry {
  constructor(type, time, kindName, kind, profilerEntry) {
    super(type, time, profilerEntry);
    this._kind = kind;
    this._kindName = kindName;
    this._feedbackVector = undefined;
  }

  get kind() {
    return this._kind;
  }

  get isBuiltinKind() {
    return this._kindName === 'Builtin';
  }

  get isBytecodeKind() {
    return this._kindName === 'Unopt';
  }

  get kindName() {
    return this._kindName;
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

  get feedbackVector() {
    return this._feedbackVector;
  }

  setFeedbackVector(fbv) {
    if (this._feedbackVector) {
      throw new Error('Double setting FeedbackVector');
    }
    this._feedbackVector = fbv;
  }

  toString() {
    return `Code(${this.type})`;
  }

  get toolTipDict() {
    const dict = super.toolTipDict;
    dict.size = formatBytes(dict.size);
    dict.source = new CodeString(dict.source);
    dict.code = new CodeString(dict.code);
    return dict;
  }

  static get propertyNames() {
    return [
      'functionName', 'sourcePosition', 'kindName', 'size', 'type', 'kind',
      'script', 'source', 'code', 'feedbackVector', 'variants'
    ];
  }
}

export class FeedbackVectorEntry extends LogEntry {
  constructor(
      timestamp, codeEntry, fbvAddress, length, optimizationMarker,
      optimizationTier, invocationCount, profilerTicks, string) {
    super('FeedbackVector', timestamp);
    this._length = length;
    this._code = codeEntry;
    this._string = string;
    this._optimizationMarker = optimizationMarker;
    this._optimizationTier = optimizationTier;
    this._invocationCount = invocationCount;
    this._profilerTicks = profilerTicks;
  }

  toString() {
    return `FeedbackVector(l=${this.length})`
  }

  get length() {
    return this._length;
  }

  get code() {
    return this._code;
  }

  get string() {
    return this._string;
  }

  get optimizationMarker() {
    return this._optimizationMarker;
  }

  get optimizationTier() {
    return this._optimizationTier;
  }

  get invocationCount() {
    return this._invocationCount;
  }

  get profilerTicks() {
    return this._profilerTicks;
  }

  static get propertyNames() {
    return [
      'length', 'length', 'code', 'optimizationMarker', 'optimizationTier',
      'invocationCount', 'profilerTicks', 'string'
    ];
  }
}

export class SharedLibLogEntry extends CodeLikeLogEntry {
  constructor(profilerEntry) {
    super('SHARED_LIB', 0, profilerEntry);
  }

  get name() {
    return this._entry.name;
  }

  toString() {
    return `SharedLib`;
  }

  static get propertyNames() {
    return ['name'];
  }
}
