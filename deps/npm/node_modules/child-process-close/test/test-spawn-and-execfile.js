
require('../index');

var assert = require('assert'),
    spawn = require('child_process').spawn;
    execFile = require('child_process').execFile;


var cp1 = spawn(process.execPath, ['worker-spawn']);
check(cp1);

var cp2 = execFile(process.execPath, ['worker-spawn'], function(err) {
  assert(!err);
});
check(cp2);


function check(cp) {
  var gotExit = false,
      gotClose = false,
      stdoutData = '',
      stdoutEnd = false,
      stderrData = '',
      stderrEnd = false;

  cp.stdout.setEncoding('ascii');

  cp.stdout.on('data', function(data) {
    assert(!stdoutEnd);
    stdoutData += data;
  });

  cp.stdout.on('end', function(data) {
    assert(!stdoutEnd)
    assert.strictEqual(stdoutData.length, 100000);
    stdoutEnd = true;
  });

  cp.stderr.setEncoding('ascii');

  cp.stderr.on('data', function(data) {
    stderrData += data;
  });

  cp.stderr.on('end', function(data) {
    assert(!stderrEnd)
    assert.strictEqual(stderrData.length, 100000);
    stderrEnd = true;
  });

  cp.on('exit', function(code, signal) {
    assert.strictEqual(code, 0);
    assert(!signal);
    assert(!gotExit);
    assert(!gotClose);
    gotExit = true;
  });

  cp.on('close', function(code, signal) {
    assert.strictEqual(code, 0);
    assert(!signal);
    assert(!cp.stdout || stdoutEnd);
    assert(!cp.stderr || stderrEnd);
    assert(gotExit);
    assert(!gotClose);
    gotClose = true;
  });

  process.on('exit', function() {
    assert(gotExit);
    assert(gotClose);
  });
}
