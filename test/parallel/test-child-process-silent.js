'use strict';
require('../common');
var assert = require('assert');
var childProcess = require('child_process');

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
  child.channel.close();
  child.channel = null;

  child.on('exit', function() {
    process.exit(0);
  });

} else {
  // testcase | start parent && child IPC test

  // testing: is stderr and stdout piped to parent
  var args = [process.argv[1], 'parent'];
  var parent = childProcess.spawn(process.execPath, args);

  //got any stderr or std data
  var stdoutData = false;
  parent.stdout.on('data', function() {
    stdoutData = true;
  });
  var stderrData = false;
  parent.stdout.on('data', function() {
    stderrData = true;
  });

  // testing: do message system work when using silent
  const child = childProcess.fork(process.argv[1], ['ipc'], {silent: true});

  // Manual pipe so we will get errors
  child.stderr.pipe(process.stderr, {end: false});
  child.stdout.pipe(process.stdout, {end: false});

  var childSending = false;
  var childReciveing = false;
  child.on('message', function(message) {
    if (childSending === false) {
      childSending = (message === 'message from child');
    }

    if (childReciveing === false) {
      childReciveing = (message === 'got message from master');
    }

    if (childReciveing === true) {
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
    assert.ok(!stdoutData, 'The stdout socket was piped to parent');
    assert.ok(!stderrData, 'The stderr socket was piped to parent');

    // Check message system
    assert.ok(childSending, 'The child was able to send a message');
    assert.ok(childReciveing, 'The child was able to receive a message');
  });
}
