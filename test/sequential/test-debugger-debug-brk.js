'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
const assert = require('assert');
const spawn = require('child_process').spawn;

const script = common.fixturesDir + '/empty.js';

function fail() {
  assert(0); // `node --debug-brk script.js` should not quit
}

function test(arg) {
  const child = spawn(process.execPath, [arg, script]);
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
test('--inspect-brk');
test('--inspect-brk=9230');
