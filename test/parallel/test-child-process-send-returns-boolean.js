'use strict';
const common = require('../common');

// subprocess.send() will return false if the channel has closed or when the
// backlog of unsent messages exceeds a threshold that makes it unwise to send
// more. Otherwise, the method returns true.

const assert = require('assert');
const net = require('net');
const { fork, spawn } = require('child_process');
const fixtures = require('../common/fixtures');

// Just a script that stays alive (does not listen to `process.on('message')`).
const subScript = fixtures.path('child-process-persistent.js');

{
  // Test `send` return value on `fork` that opens and IPC by default.
  const n = fork(subScript);
  // `subprocess.send` should always return `true` for the first send.
  const rv = n.send({ h: 'w' }, assert.ifError);
  assert.strictEqual(rv, true);
  n.kill('SIGKILL');
}

{
  // Test `send` return value on `spawn` and saturate backlog with handles.
  // Call `spawn` with options that open an IPC channel.
  const spawnOptions = { stdio: ['pipe', 'pipe', 'pipe', 'ipc'] };
  const s = spawn(process.execPath, [subScript], spawnOptions);

  const server = net.createServer(common.mustNotCall()).listen(0, () => {
    const handle = server._handle;

    // Sending a handle and not giving the tickQueue time to acknowledge should
    // create the internal backlog, but leave it empty.
    const rv1 = s.send('one', handle, assert.ifError);
    assert.strictEqual(rv1, true);
    // Since the first `send` included a handle (should be unacknowledged),
    // we can safely queue up only one more message.
    const rv2 = s.send('two', assert.ifError);
    assert.strictEqual(rv2, true);
    // The backlog should now be indicate to backoff.
    const rv3 = s.send('three', assert.ifError);
    assert.strictEqual(rv3, false);
    const rv4 = s.send('four', (err) => {
      assert.ifError(err);
      // `send` queue should have been drained.
      const rv5 = s.send('5', handle, assert.ifError);
      assert.strictEqual(rv5, true);

      // End test and cleanup.
      s.kill();
      handle.close();
      server.close();
    });
    assert.strictEqual(rv4, false);
  });
}
