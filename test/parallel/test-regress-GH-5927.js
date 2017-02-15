'use strict';
require('../common');
const assert = require('assert');
const readline = require('readline');

const rl = readline.createInterface(process.stdin, process.stdout);
rl.resume();

let hasPaused = false;

const origPause = rl.pause;
rl.pause = function() {
  hasPaused = true;
  origPause.apply(this, arguments);
};

const origSetRawMode = rl._setRawMode;
rl._setRawMode = function(mode) {
  assert.ok(hasPaused);
  origSetRawMode.apply(this, arguments);
};

rl.close();
