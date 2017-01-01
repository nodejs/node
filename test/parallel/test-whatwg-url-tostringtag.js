'use strict';

require('../common');
const assert = require('assert');
const URL = require('url').URL;
const toString = Object.prototype.toString;

const url = new URL('http://example.org');
const sp = url.searchParams;

const test = [
  [toString.call(url), 'URL'],
  [toString.call(Object.getPrototypeOf(url)), 'URLPrototype'],
  [toString.call(sp), 'URLSearchParams'],
  [toString.call(Object.getPrototypeOf(sp)), 'URLSearchParamsPrototype']
];

test.forEach((row) => {
  assert.strictEqual(row[0], `[object ${row[1]}]`,
                     `${row[0]} !== [object ${row[1]}]`);
});
