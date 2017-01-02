'use strict';
// Test that a Linux specific quirk in the handle passing protocol is handled
// correctly. See https://github.com/joyent/node/issues/5330 for details.

const common = require('../common');
const assert = require('assert');
const net = require('net');
const spawn = require('child_process').spawn;

if (process.argv[2] === 'worker')
  worker();
else
  master();

function master() {
  // spawn() can only create one IPC channel so we use stdin/stdout as an
  // ad-hoc command channel.
  const proc = spawn(process.execPath, [__filename, 'worker'], {
    stdio: ['pipe', 'pipe', 'pipe', 'ipc']
  });
  let handle = null;
  proc.on('exit', () => {
    handle.close();
  });
  proc.stdout.on('data', common.mustCall((data) => {
    assert.strictEqual(data.toString(), 'ok\r\n');
    net.createServer(common.fail).listen(0, function() {
      handle = this._handle;
      proc.send('one');
      proc.send('two', handle);
      proc.send('three');
      proc.stdin.write('ok\r\n');
    });
  }));
  proc.stderr.pipe(process.stderr);
}

function worker() {
  process.channel.readStop();  // Make messages batch up.
  process.stdout.ref();
  process.stdout.write('ok\r\n');
  process.stdin.once('data', common.mustCall((data) => {
    assert.strictEqual(data.toString(), 'ok\r\n');
    process.channel.readStart();
  }));
  let n = 0;
  process.on('message', common.mustCall((msg, handle) => {
    n += 1;
    if (n === 1) {
      assert.strictEqual(msg, 'one');
      assert.strictEqual(handle, undefined);
    } else if (n === 2) {
      assert.strictEqual(msg, 'two');
      assert.ok(handle !== null && typeof handle === 'object');
      handle.close();
    } else if (n === 3) {
      assert.strictEqual(msg, 'three');
      assert.strictEqual(handle, undefined);
      process.exit();
    }
  }, 3));
}
