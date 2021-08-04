// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {LogEntry} from './log.mjs';

export class ApiLogEntry extends LogEntry {
  constructor(type, time, name, argument) {
    super(type, time);
    this._name = name;
    this._argument = argument;
  }

  get name() {
    return this._name;
  }

  get argument() {
    return this._argument;
  }

  toString() {
    return `Api(${this.type})`;
  }

  toStringLong() {
    return `Api(${this.type}): ${this._name}`;
  }

  static get propertyNames() {
    return ['type', 'name', 'argument'];
  }
}
