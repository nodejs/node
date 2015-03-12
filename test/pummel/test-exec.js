var common = require('../common');
var assert = require('assert');
var exec = require('child_process').exec;

if (process.platform !== 'win32') {
  // Unix.
  var SLEEP3_COMMAND = "sleep 3";
} else {
  // Windows: `choice` is a command built into cmd.exe. Use another cmd process
  // to create a process tree, so we can catch bugs related to it.
  var SLEEP3_COMMAND = "cmd /c choice /t 3 /c X /d X";
}


var success_count = 0;
var error_count = 0;


exec('"' + process.execPath + '" -p -e process.versions',
     function(err, stdout, stderr) {
  if (err) {
    error_count++;
    console.log('error!: ' + err.code);
    console.log('stdout: ' + JSON.stringify(stdout));
    console.log('stderr: ' + JSON.stringify(stderr));
    assert.equal(false, err.killed);
  } else {
    success_count++;
    console.dir(stdout);
  }
});


exec('thisisnotavalidcommand', function(err, stdout, stderr) {
  if (err) {
    error_count++;
    assert.equal('', stdout);
    assert.equal(true, err.code != 0);
    assert.equal(false, err.killed);
    assert.strictEqual(null, err.signal);
    console.log('error code: ' + err.code);
    console.log('stdout: ' + JSON.stringify(stdout));
    console.log('stderr: ' + JSON.stringify(stderr));
  } else {
    success_count++;
    console.dir(stdout);
    assert.equal(true, stdout != '');
  }
});



var sleeperStart = new Date();
exec(SLEEP3_COMMAND, { timeout: 50 }, function(err, stdout, stderr) {
  var diff = (new Date()) - sleeperStart;
  console.log('\'sleep 3\' with timeout 50 took %d ms', diff);
  assert.ok(diff < 500);
  assert.ok(err);
  assert.ok(err.killed);
  assert.equal(err.signal, 'SIGTERM');
});




var startSleep3 = new Date();
var killMeTwice = exec(SLEEP3_COMMAND, {timeout: 1000}, killMeTwiceCallback);

process.nextTick(function() {
  console.log('kill pid %d', killMeTwice.pid);
  // make sure there is no race condition in starting the process
  // the PID SHOULD exist directly following the exec() call.
  assert.equal('number', typeof killMeTwice._handle.pid);
  // Kill the process
  killMeTwice.kill();
});

function killMeTwiceCallback(err, stdout, stderr) {
  var diff = (new Date()) - startSleep3;
  // We should have already killed this process. Assert that the timeout still
  // works and that we are getting the proper callback parameters.
  assert.ok(err);
  assert.ok(err.killed);
  assert.equal(err.signal, 'SIGTERM');

  // the timeout should still be in effect
  console.log('\'sleep 3\' was already killed. Took %d ms', diff);
  assert.ok(diff < 1500);
}


exec('python -c "print 200000*\'C\'"', {maxBuffer: 1000},
     function(err, stdout, stderr) {
       assert.ok(err);
       assert.ok(/maxBuffer/.test(err.message));
     });


process.on('exit', function() {
  assert.equal(1, success_count);
  assert.equal(1, error_count);
});
