var assert = require('assert');
var N = 2;
var tickCount = 0;
var exceptionCount = 0;

function cb() {
  ++tickCount;
  throw new Error();
}

for (var i = 0; i < N; ++i) {
  process.nextTick(cb);
}

process.on('uncaughtException', function() {
  ++exceptionCount;
});

process.on('exit', function() {
  process.removeAllListeners('uncaughtException');
  assert.equal(tickCount, N);
  assert.equal(exceptionCount, N);
});
