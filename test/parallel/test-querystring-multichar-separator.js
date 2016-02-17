'use strict';
require('../common');
const assert = require('assert');
const qs = require('querystring');

assert.deepEqual(
  qs.parse('foo=>bar&&bar=>baz', '&&', '=>'),
  {foo: 'bar', bar: 'baz'}
);

assert.strictEqual(
  qs.stringify({foo: 'bar', bar: 'baz'}, '&&', '=>'),
  'foo=>bar&&bar=>baz'
);

assert.deepEqual(
  qs.parse('foo==>bar, bar==>baz', ', ', '==>'),
  {foo: 'bar', bar: 'baz'}
);

assert.strictEqual(
  qs.stringify({foo: 'bar', bar: 'baz'}, ', ', '==>'),
  'foo==>bar, bar==>baz'
);
