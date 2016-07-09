'use strict';
// Test that a Linux specific quirk in the handle passing protocol is handled
// correctly. See https://github.com/joyent/node/issues/5330 for details.

var common = require('../common');
var assert = require('assert');
var net = require('net');
var spawn = require('child_process').spawn;

if (process.argv[2] === 'worker')
  worker();
else
  master();

function master() {
  // spawn() can only create one IPC channel so we use stdin/stdout as an
  // ad-hoc command channel.
  var proc = spawn(process.execPath, [__filename, 'worker'], {
    stdio: ['pipe', 'pipe', 'pipe', 'ipc']
  });
  var handle = null;
  proc.on('exit', function() {
    handle.close();
  });
  proc.stdout.on('data', function(data) {
    assert.equal(data, 'ok\r\n');
    net.createServer(common.fail).listen(0, function() {
      handle = this._handle;
      proc.send('one');
      proc.send('two', handle);
      proc.send('three');
      proc.stdin.write('ok\r\n');
    });
  });
  proc.stderr.pipe(process.stderr);
}

function worker() {
  process._channel.readStop();  // Make messages batch up.
  process.stdout.ref();
  process.stdout.write('ok\r\n');
  process.stdin.once('data', function(data) {
    assert.equal(data, 'ok\r\n');
    process._channel.readStart();
  });
  var n = 0;
  process.on('message', function(msg, handle) {
    n += 1;
    if (n === 1) {
      assert.equal(msg, 'one');
      assert.equal(handle, undefined);
    } else if (n === 2) {
      assert.equal(msg, 'two');
      assert.equal(typeof handle, 'object');  // Also matches null, therefore...
      assert.ok(handle);                      // also check that it's truthy.
      handle.close();
    } else if (n === 3) {
      assert.equal(msg, 'three');
      assert.equal(handle, undefined);
      process.exit();
    }
  });
}
