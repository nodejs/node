'use strict';

// Verifies that uploading data from a client works

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const fs = require('fs');
const fixtures = require('../common/fixtures');

const loc = fixtures.path('person-large.jpg');

assert(fs.existsSync(loc));

fs.readFile(loc, common.mustSucceed((data) => {
  const server = http2.createServer();

  server.on('stream', common.mustCall((stream) => {
    // Wait for some data to come through.
    setImmediate(() => {
      stream.on('close', common.mustCall(() => {
        assert.strictEqual(stream.rstCode, 0);
      }));

      stream.respond({ ':status': 400 });
      stream.end();
    });
  }));

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);

    const req = client.request({ ':method': 'POST' });
    req.on('response', common.mustCall((headers) => {
      assert.strictEqual(headers[':status'], 400);
    }));

    req.resume();
    req.on('end', common.mustCall(() => {
      server.close();
      client.close();
    }));

    const str = fs.createReadStream(loc);
    str.pipe(req);
  }));
}));
