'use strict';

// Tests below are not from WPT.

require('../common');
const assert = require('assert');

const toString = Object.prototype.toString;

const url = new URL('http://example.org');
const sp = url.searchParams;
const spIterator = sp.entries();

const test = [
  [url, 'URL'],
  [sp, 'URLSearchParams'],
  [spIterator, 'URLSearchParams Iterator'],
  // Web IDL spec says we have to return 'URLPrototype', but it is too
  // expensive to implement; therefore, use Chrome's behavior for now, until
  // spec is changed.
  [Object.getPrototypeOf(url), 'URL'],
  [Object.getPrototypeOf(sp), 'URLSearchParams'],
  [Object.getPrototypeOf(spIterator), 'URLSearchParams Iterator'],
];

test.forEach(([obj, expected]) => {
  assert.strictEqual(obj[Symbol.toStringTag], expected,
                     `${obj[Symbol.toStringTag]} !== ${expected}`);
  const str = toString.call(obj);
  assert.strictEqual(str, `[object ${expected}]`,
                     `${str} !== [object ${expected}]`);
});
