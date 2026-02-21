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
    /** @type {CodeEntry} */
    this._entry = entry;
    /** @type {string} */
    this._reason = deoptReason;
    /** @type {SourcePosition} */
    this._location = deoptLocation;
    /** @type {number} */
    this._scriptOffset = scriptOffset;
    /** @type {number} */
    this._instructionStart = instructionStart;
    /** @type {number} */
    this._codeSize = codeSize;
    /** @type {string} */
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
    /** @type {CodeEntry} */
    this._entry = profilerEntry;
    profilerEntry.logEntry = this;
    /** @type {LogEntry[]} */
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
  constructor(type, time, kindName, kind, name, profilerEntry) {
    super(type, time, profilerEntry);
    this._kind = kind;
    /** @type {string} */
    this._kindName = kindName;
    /** @type {?FeedbackVectorEntry} */
    this._feedbackVector = undefined;
    /** @type {string} */
    this._name = name;
  }

  get name() {
    return this._name;
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

  get isScript() {
    return this._type.startsWith('Script');
  }

  get kindName() {
    return this._kindName;
  }

  get functionName() {
    return this._entry.functionName ?? this._entry.getRawName();
  }

  get shortName() {
    if (this.isScript) {
      let url = this.sourcePosition?.script?.name ?? '';
      return url.substring(url.lastIndexOf('/') + 1);
    }
    return this.functionName;
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
    const entries = Array.from(this.entry?.sfi?.codeEntries ?? []);
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
    if (dict.code) dict.code = new CodeString(dict.code);
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

export class BaseCPPLogEntry extends CodeLikeLogEntry {
  constructor(prefix, profilerEntry) {
    super(prefix, 0, profilerEntry);
  }

  get name() {
    return this._entry.name;
  }

  get shortName() {
    let name = this.name;
    return name.substring(name.lastIndexOf('/') + 1);
  }

  toString() {
    return `SharedLib`;
  }

  static get propertyNames() {
    return ['name'];
  }
}

export class CPPCodeLogEntry extends BaseCPPLogEntry {
  constructor(profilerEntry) {
    super('CPP', profilerEntry);
  }
}

export class SharedLibLogEntry extends BaseCPPLogEntry {
  constructor(profilerEntry) {
    super('SHARED_LIB', profilerEntry);
  }
}
