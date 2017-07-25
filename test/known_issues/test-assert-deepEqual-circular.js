'use strict';
require('../common');

// Refs: https://github.com/nodejs/node/issues/14441

const assert = require('assert');

{
  const a = {};
  const b = {};
  a.a = a;
  b.a = {};
  b.a.a = a;
  assert.deepEqual(b, a); // eslint-disable-line no-restricted-properties
  assert.deepEqual(a, b); // eslint-disable-line no-restricted-properties
}

{
  const a = new Set();
  const b = new Set();
  const c = new Set();
  a.add(a);
  b.add(b);
  c.add(a);
  assert.deepEqual(c, b); // eslint-disable-line no-restricted-properties
  assert.deepEqual(b, c); // eslint-disable-line no-restricted-properties
}
