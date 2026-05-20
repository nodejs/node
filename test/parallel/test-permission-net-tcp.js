'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');
const net = require('net');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');

const file = fixtures.path('permission', 'net-tcp.js');

const server = net.createServer().listen(0, common.mustCall(() => {
  const port = server.address().port;

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
        PORT: `${port}`,
        HOST: 'localhost',
      },
    }
  );

  server.close();
  assert.strictEqual(status, 0, stderr.toString());
}));
