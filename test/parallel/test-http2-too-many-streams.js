'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const Countdown = require('../common/countdown');
const http2 = require('http2');
const assert = require('assert');

// Test that the maxConcurrentStreams setting is strictly enforced

const server = http2.createServer({ settings: { maxConcurrentStreams: 1 } });

let c = 0;

server.on('stream', common.mustCall((stream) => {
  // Because we only allow one open stream at a time,
  // c should never be greater than 1.
  assert.strictEqual(++c, 1);
  stream.respond();
  // Force some asynchronous stuff.
  setImmediate(() => {
    stream.end('ok');
    assert.strictEqual(--c, 0);
  });
}, 3));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  const countdown = new Countdown(3, common.mustCall(() => {
    server.close();
    client.destroy();
  }));

  client.on('remoteSettings', common.mustCall(() => {
    assert.strictEqual(client.remoteSettings.maxConcurrentStreams, 1);

    {
      const req = client.request();
      req.resume();
      req.on('close', () => {
        countdown.dec();

        setImmediate(() => {
          const req = client.request();
          req.resume();
          req.on('close', () => countdown.dec());
        });
      });
    }

    {
      const req = client.request();
      req.resume();
      req.on('close', () => countdown.dec());
    }
  }));
}));
