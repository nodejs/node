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

fs.readFile(loc, common.mustCall((err, data) => {
  assert.ifError(err);

  const server = http2.createServer(common.mustCall((req, res) => {
    setImmediate(() => {
      res.writeHead(400);
      res.end();
    });
  }));
  server.on('close', common.mustCall());

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    client.on('close', common.mustCall());

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
