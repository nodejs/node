'use strict';
require('../common');
const assert = require('assert');

const N = 3;
var count = 0;
function next() {
  const immediate = setImmediate(function() {
    clearImmediate(immediate);
    ++count;
  });
}
for (var i = 0; i < N; ++i)
  next();

process.on('exit', () => {
  assert.strictEqual(count, N, `Expected ${N} immediate callback executions`);
});
