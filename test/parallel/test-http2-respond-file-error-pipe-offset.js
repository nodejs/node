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
    offset: 10,
    onError(err) {
      common.expectsError({
        code: 'ERR_HTTP2_SEND_FILE_NOSEEK',
        name: 'Error',
        message: 'Offset or length can only be specified for regular files'
      })(err);

      stream.respond({ ':status': 404 });
      stream.end();
    },
    statCheck: common.mustNotCall()
  });
});
server.listen(0, () => {

  const client = http2.connect(`http://localhost:${server.address().port}`);
  const req = client.request();

  req.on('response', common.mustCall((headers) => {
    assert.strictEqual(headers[':status'], 404);
  }));
  req.on('data', common.mustNotCall());
  req.on('end', common.mustCall(() => {
    client.close();
    server.close();
  }));
  req.end();
});

fs.writeFile(pipeName, 'Hello, world!\n', common.mustCall());
