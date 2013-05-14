
require('../index');

var assert = require('assert'),
    fork = require('child_process').fork;

var cp = fork('worker-fork');

var gotMessage = false,
    gotExit = false,
    gotClose = false;

cp.on('message', function(msg) {
  assert.strictEqual(msg, 'hello');
  assert(!gotMessage);
  assert(!gotClose);
  gotMessage = true;
});

cp.on('exit', function(code, signal) {
  assert.strictEqual(code, 0);
  assert(!signal);
  assert(!gotExit);
  assert(!gotClose);
  gotExit = true;
});

cp.on('close', function(code, signal) {
  assert.strictEqual(code, 0);
  assert(!signal);
  assert(gotExit);
  assert(gotMessage);
  assert(!gotClose);
  gotClose = true;
});

process.on('exit', function() {
  assert(gotMessage);
  assert(gotExit);
  assert(gotClose);
});
