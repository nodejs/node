'use strict';
const common = require('../common');
const assert = require('assert');
const repl = require('repl');
let terminalExit = 0;
let regularExit = 0;

// Create a dummy stream that does nothing
const stream = new common.ArrayStream();

function testTerminalMode() {
  var r1 = repl.start({
    input: stream,
    output: stream,
    terminal: true
  });

  process.nextTick(function() {
    // manually fire a ^D keypress
    stream.emit('data', '\u0004');
  });

  r1.on('exit', function() {
    // should be fired from the simulated ^D keypress
    terminalExit++;
    testRegularMode();
  });
}

function testRegularMode() {
  var r2 = repl.start({
    input: stream,
    output: stream,
    terminal: false
  });

  process.nextTick(function() {
    stream.emit('end');
  });

  r2.on('exit', function() {
    // should be fired from the simulated 'end' event
    regularExit++;
  });
}

process.on('exit', function() {
  assert.equal(terminalExit, 1);
  assert.equal(regularExit, 1);
});


// start
testTerminalMode();
