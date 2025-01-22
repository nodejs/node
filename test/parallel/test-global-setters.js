/* eslint-disable strict */
require('../common');
const assert = require('assert');
const _process = require('process');
const { Buffer: _Buffer } = require('buffer');

assert.strictEqual(process, _process);
// eslint-disable-next-line no-global-assign
process = 'asdf';
assert.strictEqual(process, 'asdf');
assert.strictEqual(globalThis.process, 'asdf');
globalThis.process = _process;
assert.strictEqual(process, _process);
assert.strictEqual(
  typeof Object.getOwnPropertyDescriptor(globalThis, 'process').get,
  'function');

assert.strictEqual(Buffer, _Buffer);
// eslint-disable-next-line no-global-assign
Buffer = 'asdf';
assert.strictEqual(Buffer, 'asdf');
assert.strictEqual(globalThis.Buffer, 'asdf');
globalThis.Buffer = _Buffer;
assert.strictEqual(Buffer, _Buffer);
assert.strictEqual(
  typeof Object.getOwnPropertyDescriptor(globalThis, 'Buffer').get,
  'function');
