'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

const server = h2.createServer();

// We use the lower-level API here
server.on('stream', common.mustCall(onStream));

const status101regex =
  /^HTTP status code 101 \(Switching Protocols\) is forbidden in HTTP\/2$/;
const afterRespondregex =
  /^Cannot specify additional headers after response initiated$/;

function onStream(stream, headers, flags) {

  assert.throws(() => stream.additionalHeaders({ ':status': 201 }),
                {
                  code: 'ERR_HTTP2_INVALID_INFO_STATUS',
                  name: 'RangeError',
                  message: /^Invalid informational status code: 201$/
                });

  assert.throws(() => stream.additionalHeaders({ ':status': 101 }),
                {
                  code: 'ERR_HTTP2_STATUS_101',
                  name: 'Error',
                  message: status101regex
                });

  assert.throws(
    () => stream.additionalHeaders({ ':method': 'POST' }),
    {
      code: 'ERR_HTTP2_INVALID_PSEUDOHEADER',
      name: 'TypeError',
      message: '":method" is an invalid pseudoheader or is used incorrectly'
    }
  );

  // Can send more than one
  stream.additionalHeaders({ ':status': 100 });
  stream.additionalHeaders({ ':status': 100 });

  stream.respond({
    'content-type': 'text/html',
    ':status': 200
  });

  assert.throws(() => stream.additionalHeaders({ abc: 123 }),
                {
                  code: 'ERR_HTTP2_HEADERS_AFTER_RESPOND',
                  name: 'Error',
                  message: afterRespondregex
                });

  stream.end('hello world');
}

server.listen(0);

server.on('listening', common.mustCall(() => {

  const client = h2.connect(`http://localhost:${server.address().port}`);

  const req = client.request({ ':path': '/' });

  // The additionalHeaders method does not exist on client stream
  assert.strictEqual(req.additionalHeaders, undefined);

  // Additional informational headers
  req.on('headers', common.mustCall((headers) => {
    assert.notStrictEqual(headers, undefined);
    assert.strictEqual(headers[':status'], 100);
  }, 2));

  // Response headers
  req.on('response', common.mustCall((headers) => {
    assert.notStrictEqual(headers, undefined);
    assert.strictEqual(headers[':status'], 200);
    assert.strictEqual(headers['content-type'], 'text/html');
  }));

  req.resume();

  req.on('end', common.mustCall(() => {
    server.close();
    client.close();
  }));
  req.end();

}));
