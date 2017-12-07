'use strict';
require('../common');

// Trivial test of fs.readFile on an empty file.

const assert = require('assert');
const fs = require('fs');
const fixtures = require('../common/fixtures');

const fn = fixtures.path('empty.txt');

fs.readFile(fn, function(err, data) {
  assert.ok(data);
});

fs.readFile(fn, 'utf8', function(err, data) {
  assert.strictEqual('', data);
});

assert.ok(fs.readFileSync(fn));
assert.strictEqual('', fs.readFileSync(fn, 'utf8'));
