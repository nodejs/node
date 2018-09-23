'use strict';
var common = require('../common'),
    assert = require('assert'),
    Stream = require('stream'),
    repl = require('repl'),
    terminalExit = 0,
    regularExit = 0;

// create a dummy stream that does nothing
var stream = new Stream();
stream.write = stream.pause = stream.resume = function() {};
stream.readable = stream.writable = true;

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
