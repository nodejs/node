'use strict';
require('../common');
const assert = require('assert');

const N = 5;
let n = 0;

function f() {
  if (++n < N) setTimeout(f, 5);
}
process.on('beforeExit', f);
process.on('exit', function() {
  assert.equal(n, N + 1);  // The sixth time we let it through.
});
