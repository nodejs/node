// Flags: --expose-http2
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const fs = require('fs');
const path = require('path');

// piping should work as expected with createWriteStream

const loc = path.join(common.fixturesDir, 'person.jpg');
const fn = path.join(common.tmpDir, 'http2pipe.jpg');
common.refreshTmpDir();

const server = http2.createServer();

server.on('request', common.mustCall((req, res) => {
  const dest = req.pipe(fs.createWriteStream(fn));
  dest.on('finish', common.mustCall(() => {
    assert.deepStrictEqual(fs.readFileSync(loc), fs.readFileSync(fn));
    fs.unlinkSync(fn);
    res.end();
  }));
}));

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const client = http2.connect(`http://localhost:${port}`);

  let remaining = 2;
  function maybeClose() {
    if (--remaining === 0) {
      server.close();
      client.destroy();
    }
  }

  const req = client.request({ ':method': 'POST' });
  req.on('response', common.mustCall());
  req.resume();
  req.on('end', common.mustCall(maybeClose));
  const str = fs.createReadStream(loc);
  str.on('end', common.mustCall(maybeClose));
  str.pipe(req);
}));
