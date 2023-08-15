'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const http2 = require('http2');
const fs = require('fs');
const net = require('net');

// HTTP/2 servers can listen on a named pipe.

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
const loc = fixtures.path('person-large.jpg');
const fn = tmpdir.resolve('person-large.jpg');

const server = http2.createServer();

server.on('stream', common.mustCall((stream) => {
  const dest = stream.pipe(fs.createWriteStream(fn));

  stream.on('end', common.mustCall(() => {
    stream.respond();
    stream.end();
  }));

  dest.on('finish', common.mustCall(() => {
    assert.strictEqual(fs.readFileSync(fn).length,
                       fs.readFileSync(loc).length);
  }));
}));

server.listen(common.PIPE, common.mustCall(() => {
  const client = http2.connect('http://localhost', {
    createConnection(url) {
      return net.connect(server.address());
    }
  });

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
