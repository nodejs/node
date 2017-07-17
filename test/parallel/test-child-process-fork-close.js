'use strict';
const common = require('../common');
const assert = require('assert');
const fork = require('child_process').fork;
const fixtures = require('../common/fixtures');

const cp = fork(fixtures.path('child-process-message-and-exit.js'));

let gotMessage = false;
let gotExit = false;
let gotClose = false;

cp.on('message', common.mustCall(function(message) {
  assert(!gotMessage);
  assert(!gotClose);
  assert.strictEqual(message, 'hello');
  gotMessage = true;
}));

cp.on('exit', common.mustCall(function() {
  assert(!gotExit);
  assert(!gotClose);
  gotExit = true;
}));

cp.on('close', common.mustCall(function() {
  assert(gotMessage);
  assert(gotExit);
  assert(!gotClose);
  gotClose = true;
}));
