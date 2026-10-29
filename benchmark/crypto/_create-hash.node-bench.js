'use strict';

const assert = require('node:assert');
const { bench } = require('node:bench');
const { createHash } = require('node:crypto');
const path = require('node:path');

const n = 1e5;
const name = path.join('crypto', 'create-hash.js');

bench(name, { params: { n } }, (b) => {
  const array = [];
  for (let i = 0; i < n; ++i) array.push(null);
  b.start();
  for (let i = 0; i < n; ++i)
    array[i] = createHash('sha1');
  b.end(n);
  assert.strictEqual(typeof array[n - 1], 'object');
});
