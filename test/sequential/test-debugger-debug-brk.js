'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();
const fixtures = require('../common/fixtures');
const assert = require('assert');
const spawn = require('child_process').spawn;

// file name here doesn't actually matter since
// debugger will connect regardless of file name arg
const script = fixtures.path('empty.js');

function test(arg) {
  const child = spawn(process.execPath, ['--inspect', arg, script]);
  const argStr = child.spawnargs.join(' ');
  const fail = () => assert.fail(true, false, `'${argStr}' should not quit`);
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
