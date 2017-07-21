'use strict';

require('../common');
const assert = require('assert');

assert.ok(process.hasOwnProperty('argv0'));
assert.throws(() => {
  process.argv0 = 'anything';
}, /^TypeError: Cannot assign to read only property 'argv0' of object '#<process>'/);
assert.doesNotThrow(() => {
  delete process.argv0;
}, /^TypeError: Cannot delete property 'argv0' of object '#<process>'/);
Object.defineProperty(process, 'argv0', {value: 'some-test-value'});
assert.strictEqual(process.argv0, 'some-test-value');
