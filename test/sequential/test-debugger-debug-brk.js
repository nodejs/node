'use strict';
var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;

var script = common.fixturesDir + '/empty.js';

function fail() {
  assert(0); // `node --debug-brk script.js` should not quit
}

function test(arg) {
  var child = spawn(process.execPath, [arg, script]);
  child.on('exit', fail);

  // give node time to start up the debugger
  setTimeout(function() {
    child.removeListener('exit', fail);
    child.kill();
  }, 2000);

  process.on('exit', function() {
    assert(child.killed);
  });
}

test('--debug-brk');
test('--debug-brk=5959');
