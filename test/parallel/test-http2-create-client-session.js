'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');
const Countdown = require('../common/countdown');

const body =
  '<html><head></head><body><h1>this is some data</h2></body></html>';

const server = h2.createServer();
const count = 100;

// we use the lower-level API here
server.on('stream', common.mustCall(onStream, count));

function onStream(stream, headers, flags) {
  assert.strictEqual(headers[':scheme'], 'http');
  assert.ok(headers[':authority']);
  assert.strictEqual(headers[':method'], 'GET');
  assert.strictEqual(flags, 5);
  stream.respond({
    'content-type': 'text/html',
    ':status': 200
  });
  stream.write(body.slice(0, 20));
  stream.end(body.slice(20));
}

server.on('close', common.mustCall());

server.listen(0);

server.on('listening', common.mustCall(() => {

  const client = h2.connect(`http://localhost:${server.address().port}`);
  client.setMaxListeners(101);

  client.on('goaway', console.log);

  client.on('connect', common.mustCall(() => {
    assert(!client.encrypted);
    assert(!client.originSet);
    assert.strictEqual(client.alpnProtocol, 'h2c');
  }));

  const countdown = new Countdown(count, () => {
    client.close();
    server.close(common.mustCall());
  });

  for (let n = 0; n < count; n++) {
    const req = client.request();

    req.on('response', common.mustCall(function(headers) {
      assert.strictEqual(headers[':status'], 200);
      assert.strictEqual(headers['content-type'], 'text/html');
      assert(headers.date);
    }));

    let data = '';
    req.setEncoding('utf8');
    req.on('data', (d) => data += d);
    req.on('end', common.mustCall(() => {
      assert.strictEqual(body, data);
    }));
    req.on('close', common.mustCall(() => countdown.dec()));
  }
}));
