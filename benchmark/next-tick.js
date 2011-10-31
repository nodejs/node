// run with `time node benchmark/next-tick.js`
var assert = require('assert');

var N = 1e7;
var n = 0;

process.on('exit', function() {
  assert.equal(n, N);
});

function cb() {
  n++;
}

for (var i = 0; i < N; ++i) {
  process.nextTick(cb);
}
