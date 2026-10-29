'use strict';

const common = require('../common');
if (!common.hasCrypto) { common.skip('missing crypto'); };
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');
const https = require('https');
const fs = require('fs');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');

const file = fixtures.path('permission', 'net-https.js');

const options = {
  key: fs.readFileSync(fixtures.path('keys', 'agent1-key.pem')),
  cert: fs.readFileSync(fixtures.path('keys', 'agent1-cert.pem'))
};

const server = https.createServer(options, (req, res) => {
  res.writeHead(200);
  res.end('Hello');
}).listen(0, common.mustCall(() => {
  const port = server.address().port;
  const url = `https://localhost:${port}`;

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
