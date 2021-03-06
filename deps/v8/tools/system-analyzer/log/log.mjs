// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class LogEntry {
  constructor(type, time) {
    this._time = time;
    this._type = type;
    this.sourcePosition = undefined;
  }

  get time() {
    return this._time;
  }

  get type() {
    return this._type;
  }

  get script() {
    return this.sourcePosition?.script;
  }

  toString() {
    return `${this.constructor.name}(${this._type})`;
  }

  toStringLong() {
    return this.toString();
  }

  // Returns an Array of all possible #type values.
  static get allTypes() {
    throw new Error('Not implemented.');
  }
}