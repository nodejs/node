'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const http2 = require('http2');
const fs = require('fs');

// Piping should work as expected with createWriteStream

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
const loc = fixtures.path('person-large.jpg');
const fn = tmpdir.resolve('http2-url-tests.js');

const server = http2.createServer();

server.on('stream', common.mustCall((stream) => {
  const dest = stream.pipe(fs.createWriteStream(fn));

  dest.on('finish', () => {
    assert.strictEqual(fs.readFileSync(loc).length,
                       fs.readFileSync(fn).length);
  });
  stream.respond();
  stream.end();
}));

server.listen(0, common.mustCall(() => {
  const client = http2.connect(`http://localhost:${server.address().port}`);

  const req = client.request({ ':method': 'POST' });

  req.on('response', common.mustCall());
  req.resume();

  req.on('close', common.mustCall(() => {
    server.close();
    client.close();
  }));

  const str = fs.createReadStream(loc);
  str.on('end', common.mustCall());
  str.pipe(req);
}));
