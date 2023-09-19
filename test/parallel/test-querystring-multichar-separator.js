'use strict';
require('../common');
const assert = require('assert');
const qs = require('querystring');

function check(actual, expected) {
  assert(!(actual instanceof Object));
  assert.deepStrictEqual(Object.keys(actual).sort(),
                         Object.keys(expected).sort());
  Object.keys(expected).forEach(function(key) {
    assert.deepStrictEqual(actual[key], expected[key]);
  });
}

check(qs.parse('foo=>bar&&bar=>baz', '&&', '=>'),
      { foo: 'bar', bar: 'baz' });

check(qs.stringify({ foo: 'bar', bar: 'baz' }, '&&', '=>'),
      'foo=>bar&&bar=>baz');

check(qs.parse('foo==>bar, bar==>baz', ', ', '==>'),
      { foo: 'bar', bar: 'baz' });

check(qs.stringify({ foo: 'bar', bar: 'baz' }, ', ', '==>'),
      'foo==>bar, bar==>baz');
