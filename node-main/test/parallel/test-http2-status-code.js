'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

const codes = [ 200, 202, 300, 400, 404, 451, 500 ];
let test = 0;

const server = http2.createServer();

server.on('stream', common.mustCall((stream) => {
  const status = codes[test++];
  stream.respond({ ':status': status }, { endStream: true });
}, 7));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  let remaining = codes.length;
  function maybeClose() {
    if (--remaining === 0) {
      client.close();
      server.close();
    }
  }

  function doTest(expected) {
    const req = client.request();
    req.on('response', common.mustCall((headers) => {
      assert.strictEqual(headers[':status'], expected);
    }));
    req.resume();
    req.on('end', common.mustCall(maybeClose));
  }

  for (let n = 0; n < codes.length; n++)
    doTest(codes[n]);
}));
