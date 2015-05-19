'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path');

// if child process output to console and exit
if (process.argv[2] === 'child') {
  console.log('hello');
  for (var i = 0; i < 200; i++) {
    console.log('filler');
  }
  console.log('goodbye');
  process.exit(0);
} else {
  // parent process
  var spawn = require('child_process').spawn;

  // spawn self as child
  var child = spawn(process.argv[0], [process.argv[1], 'child']);

  var gotHello = false;
  var gotBye = false;

  child.stderr.setEncoding('utf8');
  child.stderr.on('data', function(data) {
    console.log('parent stderr: ' + data);
    assert.ok(false);
  });

  // check if we receive both 'hello' at start and 'goodbye' at end
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', function(data) {
    if (data.slice(0, 6) == 'hello\n') {
      gotHello = true;
    } else if (data.slice(data.length - 8) == 'goodbye\n') {
      gotBye = true;
    } else {
      gotBye = false;
    }
  });

  child.on('close', function(data) {
    assert(gotHello);
    assert(gotBye);
  });
}
