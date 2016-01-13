'use strict';
const assert = require('assert');
const common = require('../common');
const fork = require('child_process').fork;

var cp = fork(common.fixturesDir + '/child-process-message-and-exit.js');

let gotMessage = false;
let gotExit = false;
let gotClose = false;

cp.on('message', function(message) {
  assert(!gotMessage);
  assert(!gotClose);
  assert.strictEqual(message, 'hello');
  gotMessage = true;
});

cp.on('exit', function() {
  assert(!gotExit);
  assert(!gotClose);
  gotExit = true;
});

cp.on('close', function() {
  assert(gotMessage);
  assert(gotExit);
  assert(!gotClose);
  gotClose = true;
});

process.on('exit', function() {
  assert(gotMessage);
  assert(gotExit);
  assert(gotClose);
});
