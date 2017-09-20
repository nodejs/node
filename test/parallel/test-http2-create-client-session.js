// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');
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
  stream.end(body);
}

server.listen(0);

let expected = count;

server.on('listening', common.mustCall(function() {

  const client = h2.connect(`http://localhost:${this.address().port}`);

  const headers = { ':path': '/' };

  for (let n = 0; n < count; n++) {
    const req = client.request(headers);

    req.on('response', common.mustCall(function(headers) {
      assert.strictEqual(headers[':status'], 200, 'status code is set');
      assert.strictEqual(headers['content-type'], 'text/html',
                         'content type is set');
      assert(headers['date'], 'there is a date');
    }));

    let data = '';
    req.setEncoding('utf8');
    req.on('data', (d) => data += d);
    req.on('end', common.mustCall(() => {
      assert.strictEqual(body, data);
      if (--expected === 0) {
        server.close();
        client.destroy();
      }
    }));
    req.end();
  }

}));
