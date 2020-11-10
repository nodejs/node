'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
if (common.isWindows)
  common.skip('no mkfifo on Windows');
const child_process = require('child_process');
const path = require('path');
const fs = require('fs');
const http2 = require('http2');
const assert = require('assert');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const pipeName = path.join(tmpdir.path, 'pipe');

const mkfifo = child_process.spawnSync('mkfifo', [ pipeName ]);
if (mkfifo.error && mkfifo.error.code === 'ENOENT') {
  common.skip('missing mkfifo');
}

const server = http2.createServer();
server.on('stream', (stream) => {
  stream.respondWithFile(pipeName, {
    'content-type': 'text/plain'
  }, {
    onError: common.mustNotCall(),
    statCheck: common.mustCall()
  });
});

server.listen(0, () => {
  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();

  req.on('response', common.mustCall((headers) => {
    assert.strictEqual(headers[':status'], 200);
  }));
  let body = '';
  req.setEncoding('utf8');
  req.on('data', (chunk) => body += chunk);
  req.on('end', common.mustCall(() => {
    assert.strictEqual(body, 'Hello, world!\n');
    client.close();
    server.close();
  }));
  req.end();
});

fs.open(pipeName, 'w', common.mustCall((err, fd) => {
  assert.ifError(err);
  fs.writeSync(fd, 'Hello, world!\n');
  fs.closeSync(fd);
}));
