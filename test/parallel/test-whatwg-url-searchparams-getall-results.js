'use strict';

require('../common');
const assert = require('node:assert');

for (const size of [0, 1, 4, 32, 256]) {
  const params = new URLSearchParams();
  const expected = [];
  for (let index = 0; index < size; index++) {
    params.append('other', `${index}`);
    const value = index % 2 === 0 ? '' : `value${index}`;
    params.append('target', value);
    expected.push(value);
  }

  const result = params.getAll('target');
  assert.deepStrictEqual(result, expected);
  result.push('extra');
  result[0] = 'changed';
  assert.deepStrictEqual(params.getAll('target'), expected);
  params.append('target', 'last');
  assert.deepStrictEqual(params.getAll('target'), [...expected, 'last']);
  assert.deepStrictEqual(params.getAll('missing'), []);
}
