// Flags: --frozen-intrinsics
'use strict';
require('../common');
const assert = require('assert');

assert.throws(
  () => Object.defineProperty = 'asdf',
  TypeError
);

// Ensure we can extend Console
{
  const logs = [];
  class ExtendedConsole extends console.Console {
    constructor() {
      super();

      this.log = (msg) => logs.push(msg);
    }
  }

  const s = new ExtendedConsole();
  s.log('custom');

  assert.strictEqual(logs.length, 1);
  assert.strictEqual(logs[0], 'custom');
}

// Ensure we can write override Object prototype properties on instances
{
  const o = {};
  o.toString = () => 'Custom toString';
  assert.strictEqual(o + 'asdf', 'Custom toStringasdf');
}
