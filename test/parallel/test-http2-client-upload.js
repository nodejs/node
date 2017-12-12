'use strict';

// Verifies that uploading data from a client works

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const fs = require('fs');
const fixtures = require('../common/fixtures');
const Countdown = require('../common/countdown');

const loc = fixtures.path('person.jpg');
let fileData;

assert(fs.existsSync(loc));

fs.readFile(loc, common.mustCall((err, data) => {
  assert.ifError(err);
  fileData = data;

  const server = http2.createServer();

  server.on('stream', common.mustCall((stream) => {
    let data = Buffer.alloc(0);
    stream.on('data', (chunk) => data = Buffer.concat([data, chunk]));
    stream.on('end', common.mustCall(() => {
      assert.deepStrictEqual(data, fileData);
    }));
    stream.respond();
    stream.end();
  }));

  server.listen(0, common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);

    const countdown = new Countdown(2, () => {
      server.close();
      client.close();
    });

    const req = client.request({ ':method': 'POST' });
    req.on('response', common.mustCall());

    req.resume();
    req.on('end', common.mustCall());

    req.on('finish', () => countdown.dec());
    const str = fs.createReadStream(loc);
    str.on('end', common.mustCall());
    str.on('close', () => countdown.dec());
    str.pipe(req);
  }));
}));
