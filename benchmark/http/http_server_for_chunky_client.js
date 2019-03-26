'use strict';

const assert = require('assert');
const http = require('http');
const { fork } = require('child_process');
const common = require('../common.js');
const { PIPE } = require('../../test/common');
const tmpdir = require('../../test/common/tmpdir');
process.env.PIPE_NAME = PIPE;

tmpdir.refresh();

const server = http.createServer((req, res) => {
  const headers = {
    'content-type': 'text/plain',
    'content-length': '2'
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
  process.argv.slice(2)
);
child.on('message', common.sendResult);
child.on('close', (code) => {
  server.close();
  assert.strictEqual(code, 0);
});
