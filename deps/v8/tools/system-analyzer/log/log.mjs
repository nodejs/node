// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class LogEntry {
  constructor(type, time) {
    /** @type {number} */
    this._time = time;
    this._type = type;
    /** @type {?SourcePosition} */
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
    let name = this.constructor.name;
    const index = name.lastIndexOf('LogEntry');
    if (index > 0) {
      name = name.substr(0, index);
    }
    return `${name}(${this._type})`;
  }

  get toolTipDict() {
    const toolTipDescription = {
      __proto__: null,
      __this__: this,
      title: this.toString()
    };
    for (let key of this.constructor.propertyNames) {
      toolTipDescription[key] = this[key];
    }

    return toolTipDescription;
  }

  // Returns an Array of all possible #type values.
  /**  @return {string[]} */
  static get allTypes() {
    throw new Error('Not implemented.');
  }

  // Returns an array of public property names.
  /**  @return {string[]} */
  static get propertyNames() {
    throw new Error('Not implemented.');
  }
}
