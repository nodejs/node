'use strict';
require('../common');
const assert = require('assert');
const { test } = require('node:test');

const defaultStartMessage = 'Expected values to be strictly deep-equal:\n' +
  '+ actual - expected\n' +
  '\n';

test('Handle error causes', () => {
  assert.throws(() => {
    assert.deepStrictEqual(new Error('a', { cause: new Error('x') }), new Error('a', { cause: new Error('y') }));
  }, { message: defaultStartMessage + '  [Error: a] {\n' +
    '+   [cause]: [Error: x]\n' +
    '-   [cause]: [Error: y]\n' +
    '  }' });

  assert.throws(() => {
    assert.deepStrictEqual(new Error('a'), new Error('a', { cause: new Error('y') }));
  }, { message: defaultStartMessage + '+ [Error: a]\n' +
    '- [Error: a] {\n' +
    '-   [cause]: [Error: y]\n' +
    '- }' });

  assert.notDeepStrictEqual(new Error('a', { cause: new Error('x') }), new Error('a', { cause: new Error('y') }));
});
