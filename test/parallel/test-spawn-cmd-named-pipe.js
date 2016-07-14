'use strict';
const common = require('../common');
const assert = require('assert');

// This test is intended for Windows only
if (!common.isWindows) {
  common.skip('this test is Windows-specific.');
  return;
}

if (!process.argv[2]) {
  // parent
  const net = require('net');
  const spawn = require('child_process').spawn;
  const path = require('path');

  const pipeNamePrefix = path.basename(__filename) + '.' + process.pid;
  const stdinPipeName = '\\\\.\\pipe\\' + pipeNamePrefix + '.stdin';
  const stdoutPipeName = '\\\\.\\pipe\\' + pipeNamePrefix + '.stdout';

  const stdinPipeServer = net.createServer(function(c) {
    c.on('end', common.mustCall(function() {
    }));
    c.end('hello');
  });
  stdinPipeServer.listen(stdinPipeName);

  const output = [];

  const stdoutPipeServer = net.createServer(function(c) {
    c.on('data', function(x) {
      output.push(x);
    });
    c.on('end', common.mustCall(function() {
      assert.strictEqual(output.join(''), 'hello');
    }));
  });
  stdoutPipeServer.listen(stdoutPipeName);

  const comspec = process.env['comspec'];
  if (!comspec || comspec.length === 0) {
    common.fail('Failed to get COMSPEC');
  }

  const args = ['/c', process.execPath, __filename, 'child',
              '<', stdinPipeName, '>', stdoutPipeName];

  const child = spawn(comspec, args);

  child.on('exit', common.mustCall(function(exitCode) {
    stdinPipeServer.close();
    stdoutPipeServer.close();
    assert.strictEqual(exitCode, 0);
  }));
} else {
  // child
  process.stdin.pipe(process.stdout);
}
