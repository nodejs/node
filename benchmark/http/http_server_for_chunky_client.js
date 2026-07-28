'use strict';

const assert = require('assert');
const http = require('http');
const { fork } = require('child_process');
const common = require('../common.js');
const { PIPE } = require('../../test/common');
const tmpdir = require('../../test/common/tmpdir');
process.env.PIPE_NAME = PIPE;

tmpdir.refresh();

// For Node.js versions below v13.3.0 this benchmark will require
// the flag --max-http-header-size 64000 in order to work properly
const server = http.createServer({ maxHeaderSize: 64000 }, (req, res) => {
  const headers = {
    'content-type': 'text/plain',
    'content-length': '2',
  };
  res.writeHead(200, headers);
  res.end('ok');
});

server.on('error', (err) => {
  throw new Error(`server error: ${err}`);
});
server.listen(PIPE);

const child = fork(
  `${__dirname}/_chunky_http_client.js`,
  process.argv.slice(2),
);
child.on('message', (data) => {
  if (data.type === 'report') {
    common.sendResult(data);
  }
});
child.on('close', (code) => {
  server.close();
  assert.strictEqual(code, 0);
});
