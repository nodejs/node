'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const h2 = require('http2');

const server = h2.createServer();

const methods = [undefined, 'GET', 'POST', 'PATCH', 'FOO', 'A B C'];
let expected = methods.length;

// We use the lower-level API here
server.on('stream', common.mustCall(onStream, expected));

function onStream(stream, headers, flags) {
  const method = headers[':method'];
  assert.notStrictEqual(method, undefined);
  assert(methods.includes(method), `method ${method} not included`);
  stream.respond({
    'content-type': 'text/html',
    ':status': 200
  });
  stream.end('hello world');
}

server.listen(0);

server.on('listening', common.mustCall(() => {

  const client = h2.connect(`http://localhost:${server.address().port}`);

  const headers = { ':path': '/' };

  methods.forEach((method) => {
    headers[':method'] = method;
    const req = client.request(headers);
    req.on('response', common.mustCall());
    req.resume();
    req.on('end', common.mustCall(() => {
      if (--expected === 0) {
        server.close();
        client.close();
      }
    }));
    req.end();
  });
}));
