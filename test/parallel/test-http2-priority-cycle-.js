'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const Countdown = require('../common/countdown');

const server = http2.createServer();
const largeBuffer = Buffer.alloc(1e4);

// Verify that a dependency cycle may exist, but that it doesn't crash anything

server.on('stream', common.mustCall((stream) => {
  stream.respond();
  setImmediate(() => {
    stream.end(largeBuffer);
  });
}, 3));
server.on('session', common.mustCall((session) => {
  session.on('priority', (id, parent, weight, exclusive) => {
    assert.strictEqual(weight, 16);
    assert.strictEqual(exclusive, false);
    switch (id) {
      case 1:
        assert.strictEqual(parent, 5);
        break;
      case 3:
        assert.strictEqual(parent, 1);
        break;
      case 5:
        assert.strictEqual(parent, 3);
        break;
      default:
        assert.fail('should not happen');
    }
  });
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  const countdown = new Countdown(3, () => {
    client.close();
    server.close();
  });

  {
    const req = client.request();
    req.priority({ parent: 5 });
    req.resume();
    req.on('close', () => countdown.dec());
  }

  {
    const req = client.request();
    req.priority({ parent: 1 });
    req.resume();
    req.on('close', () => countdown.dec());
  }

  {
    const req = client.request();
    req.priority({ parent: 3 });
    req.resume();
    req.on('close', () => countdown.dec());
  }
}));
