'use strict';
var assert = require('assert');
var common = require('../common');

var N = 5;
var n = 0;

function f() {
  if (++n < N) setTimeout(f, 5);
}
process.on('beforeExit', f);
process.on('exit', function() {
  assert.equal(n, N + 1);  // The sixth time we let it through.
});
