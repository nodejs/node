'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const Countdown = require('../common/countdown');

const server = http2.createServer();
server.on('stream', common.mustCall((stream, headers) => {
  const check = headers[':method'] === 'GET';
  assert.strictEqual(stream.endAfterHeaders, check);
  stream.on('data', common.mustNotCall());
  stream.on('end', common.mustCall());
  stream.respond();
  stream.end('ok');
}, 2));

const countdown = new Countdown(2, () => server.close());

server.listen(0, common.mustCall(() => {
  {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request();

    req.resume();
    req.on('response', common.mustCall(() => {
      assert.strictEqual(req.endAfterHeaders, false);
    }));
    req.on('end', common.mustCall(() => {
      client.close();
      countdown.dec();
    }));
  }
  {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request({ ':method': 'POST' });

    req.resume();
    req.end();
    req.on('response', common.mustCall(() => {
      assert.strictEqual(req.endAfterHeaders, false);
    }));
    req.on('end', common.mustCall(() => {
      client.close();
      countdown.dec();
    }));
  }
}));
