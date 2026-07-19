'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');
const http = require('http');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');

const file = fixtures.path('permission', 'net-http.js');

const server = http.createServer((req, res) => {
  res.writeHead(200);
  res.end('Hello');
}).listen(0, common.mustCall(() => {
  const port = server.address().port;
  const url = `http://localhost:${port}`;

  const { status, stderr } = spawnSync(
    process.execPath,
    [
      '--permission',
      '--allow-fs-read=*',
      file,
    ],
    {
      env: {
        ...process.env,
        URL: url,
      },
    }
  );

  server.close();
  assert.strictEqual(status, 0, stderr.toString());
}));
