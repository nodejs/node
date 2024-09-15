'use strict';
require('../common');
const { deepStrictEqual } = require('node:assert');
const test = require('node:test');

test('only methods from node:assert are on t.assert', (t) => {
  deepStrictEqual(Object.keys(t.assert).sort(), [
    'deepEqual',
    'deepStrictEqual',
    'doesNotMatch',
    'doesNotReject',
    'doesNotThrow',
    'equal',
    'fail',
    'ifError',
    'match',
    'notDeepEqual',
    'notDeepStrictEqual',
    'notEqual',
    'notStrictEqual',
    'ok',
    'rejects',
    'strictEqual',
    'throws',
  ]);
});

test('t.assert.ok correctly parses the stacktrace', (t) => {
  t.assert.throws(() => t.assert.ok(1 === 2), /t\.assert\.ok\(1 === 2\)/);
});
