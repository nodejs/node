'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const http2 = require('http2');
const assert = require('assert');

const server = http2.createServer();

// Check that stream ends immediately after respond on :status 204, 205 & 304

const status = [204, 205, 304];

server.on('stream', common.mustCall((stream) => {
  stream.on('close', common.mustCall(() => {
    assert.strictEqual(stream.destroyed, true);
  }));
  stream.respond({ ':status': status.shift() });
}, 3));

server.listen(0, common.mustCall(makeRequest));

function makeRequest() {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();
  req.resume();

  req.on('end', common.mustCall(() => {
    client.close();

    if (!status.length) {
      server.close();
    } else {
      makeRequest();
    }
  }));
  req.end();
}
