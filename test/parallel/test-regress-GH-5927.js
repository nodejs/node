'use strict';
require('../common');
var assert = require('assert');
var readline = require('readline');

var rl = readline.createInterface(process.stdin, process.stdout);
rl.resume();

var hasPaused = false;

var origPause = rl.pause;
rl.pause = function() {
  hasPaused = true;
  origPause.apply(this, arguments);
};

var origSetRawMode = rl._setRawMode;
rl._setRawMode = function(mode) {
  assert.ok(hasPaused);
  origSetRawMode.apply(this, arguments);
};

rl.close();
