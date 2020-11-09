'use strict';

require('../common');

// This test ensures that util.inspect logs getters
// which access this.

const assert = require('assert');

const util = require('util');

class X {
  constructor() {
    this._y = 123;
  }

  get y() {
    return this._y;
  }
}

const result = util.inspect(new X(), {
  getters: true,
  showHidden: true
});

assert.strictEqual(
  result,
  'X { _y: 123, [y]: [Getter: 123] }'
);
