'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const dns = require('dns');
const maxSendQueueSize = 10;

// Monkey patch dns.lookup() so that it always fails.
dns.lookup = function(address, family, callback) {
  callback(new Error('fake DNS'));
};

// Verify that maxSendQueueSize is respected.
{
  const socket = dgram.createSocket({ type: 'udp4', maxSendQueueSize });
  let queueErrors = 0;

  process.on('exit', () => {
    assert.strictEqual(queueErrors, 1);
  });

  socket.on('error', (err) => {
    // Ignore DNS errors.
    if (/^Error: fake DNS$/.test(err))
      return;

    if (/^Error: Maximum send queue size exceeded$/.test(err)) {
      queueErrors++;
      return;
    }

    common.fail(`Unexpected error: ${err}`);
  });

  // Fill the send queue.
  for (let i = 0; i < maxSendQueueSize; ++i)
    socket.send('foobar', common.PORT, 'localhost');

  // Pause to make sure nothing leaves the queue.
  setImmediate(() => {
    assert.strictEqual(socket._queue.length, maxSendQueueSize);
    socket.send('foobar', common.PORT, 'localhost');
    assert.strictEqual(socket._queue.length, maxSendQueueSize);
  });
}

// Verify the default behavior when no maxSendQueueSize is specified.
{
  const socket = dgram.createSocket({ type: 'udp4' });

  // Only fake DNS errors should be seen.
  socket.on('error', (err) => { assert(/^Error: fake DNS$/.test(err)); });

  for (let i = 0; i < maxSendQueueSize * 2; ++i)
    socket.send('foobar', common.PORT, 'localhost');

  assert.strictEqual(socket._queue.length, maxSendQueueSize * 2);
}
