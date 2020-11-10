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
  class ExtendedConsole extends console.Console {}

  const s = new ExtendedConsole(process.stdout);
  const logs = [];
  s.log = (msg) => logs.push(msg);
  s.log('custom');
  s.log = undefined;
  assert.strictEqual(s.log, undefined);
  assert.strictEqual(logs.length, 1);
  assert.strictEqual(logs[0], 'custom');
}

// Ensure we can write override Object prototype properties on instances
{
  const o = {};
  o.toString = () => 'Custom toString';
  assert.strictEqual(o + 'asdf', 'Custom toStringasdf');
}
