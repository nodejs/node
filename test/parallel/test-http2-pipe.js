'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const http2 = require('http2');
const fs = require('fs');
const path = require('path');
const Countdown = require('../common/countdown');

// piping should work as expected with createWriteStream

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
const loc = fixtures.path('url-tests.js');
const fn = path.join(tmpdir.path, 'http2-url-tests.js');

const server = http2.createServer();

server.on('stream', common.mustCall((stream) => {
  const dest = stream.pipe(fs.createWriteStream(fn));
  dest.on('finish', common.mustCall(() => {
    assert.strictEqual(fs.readFileSync(loc).length, fs.readFileSync(fn).length);
    fs.unlinkSync(fn);
    stream.respond();
    stream.end();
  }));
}));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const client = http2.connect(`http://localhost:${port}`);

  const countdown = new Countdown(2, common.mustCall(() => {
    server.close();
    client.destroy();
  }));

  const req = client.request({ ':method': 'POST' });
  req.on('response', common.mustCall());
  req.resume();
  req.on('end', common.mustCall(() => countdown.dec()));
  const str = fs.createReadStream(loc);
  str.on('end', common.mustCall(() => countdown.dec()));
  str.pipe(req);
}));
