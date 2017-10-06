'use strict';
require('../common');
const assert = require('assert');
const childProcess = require('child_process');

// Child pipe test
if (process.argv[2] === 'pipe') {
  process.stdout.write('stdout message');
  process.stderr.write('stderr message');

} else if (process.argv[2] === 'ipc') {
  // Child IPC test
  process.send('message from child');
  process.on('message', function() {
    process.send('got message from master');
  });

} else if (process.argv[2] === 'parent') {
  // Parent | start child pipe test

  const child = childProcess.fork(process.argv[1], ['pipe'], {silent: true});

  // Allow child process to self terminate
  child._channel.close();
  child._channel = null;

  child.on('exit', function() {
    process.exit(0);
  });

} else {
  // testcase | start parent && child IPC test

  // testing: is stderr and stdout piped to parent
  const args = [process.argv[1], 'parent'];
  const parent = childProcess.spawn(process.execPath, args);

  //got any stderr or std data
  let stdoutData = false;
  parent.stdout.on('data', function() {
    stdoutData = true;
  });
  let stderrData = false;
  parent.stderr.on('data', function() {
    stderrData = true;
  });

  // testing: do message system work when using silent
  const child = childProcess.fork(process.argv[1], ['ipc'], {silent: true});

  // Manual pipe so we will get errors
  child.stderr.pipe(process.stderr, {end: false});
  child.stdout.pipe(process.stdout, {end: false});

  let childSending = false;
  let childReceiving = false;
  child.on('message', function(message) {
    if (childSending === false) {
      childSending = (message === 'message from child');
    }

    if (childReceiving === false) {
      childReceiving = (message === 'got message from master');
    }

    if (childReceiving === true) {
      child.kill();
    }
  });
  child.send('message to child');

  // Check all values
  process.on('exit', function() {
    // clean up
    child.kill();
    parent.kill();

    // Check std(out|err) pipes
    assert.ok(!stdoutData);
    assert.ok(!stderrData);

    // Check message system
    assert.ok(childSending);
    assert.ok(childReceiving);
  });
}
