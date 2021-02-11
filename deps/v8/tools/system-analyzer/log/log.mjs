// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class LogEntry {
  _time;
  _type;
  constructor(type, time) {
    // TODO(zcankara) remove type and add empty getters to override
    this._time = time;
    this._type = type;
  }
  get time() {
    return this._time;
  }
  get type() {
    return this._type;
  }
  // Returns an Array of all possible #type values.
  static get allTypes() {
    throw new Error('Not implemented.');
  }
}