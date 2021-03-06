'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const http2 = require('http2');
const Countdown = require('../common/countdown');

const server = http2.createServer();
server.on('stream', (stream) => {
  stream.respond();
  stream.end('ok');
});

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const nextID = 2 ** 31 - 1;

  client.on('connect', () => {
    client.setNextStreamID(nextID);

    assert.strictEqual(client.state.nextStreamID, nextID);

    const countdown = new Countdown(2, () => {
      server.close();
      client.close();
    });

    {
      // This one will be ok
      const req = client.request();
      assert.strictEqual(req.id, nextID);

      req.on('error', common.mustNotCall());
      req.resume();
      req.on('end', () => countdown.dec());
    }

    {
      // This one will error because there are no more stream IDs available
      const req = client.request();
      req.on('error', common.expectsError({
        code: 'ERR_HTTP2_OUT_OF_STREAMS',
        name: 'Error',
        message:
          'No stream ID is available because maximum stream ID has been reached'
      }));
      req.on('error', () => countdown.dec());
    }
  });
}));
