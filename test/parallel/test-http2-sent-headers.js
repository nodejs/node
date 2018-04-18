'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

const server = h2.createServer();

server.on('stream', common.mustCall((stream) => {
  stream.additionalHeaders({ ':status': 102 });
  assert.strictEqual(stream.sentInfoHeaders[0][':status'], 102);

  stream.respond({ abc: 'xyz' }, { waitForTrailers: true });
  stream.on('wantTrailers', () => {
    stream.sendTrailers({ xyz: 'abc' });
  });
  assert.strictEqual(stream.sentHeaders.abc, 'xyz');
  assert.strictEqual(stream.sentHeaders[':status'], 200);
  assert.notStrictEqual(stream.sentHeaders.date, undefined);
  stream.end();
  stream.on('close', () => {
    assert.strictEqual(stream.sentTrailers.xyz, 'abc');
  });
}));

server.listen(0, common.mustCall(() => {
  const client = h2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();

  req.on('headers', common.mustCall((headers) => {
    assert.strictEqual(headers[':status'], 102);
  }));

  assert.strictEqual(req.sentHeaders[':method'], 'GET');
  assert.strictEqual(req.sentHeaders[':authority'],
                     `localhost:${server.address().port}`);
  assert.strictEqual(req.sentHeaders[':scheme'], 'http');
  assert.strictEqual(req.sentHeaders[':path'], '/');
  req.resume();
  req.on('close', () => {
    server.close();
    client.close();
  });
}));
