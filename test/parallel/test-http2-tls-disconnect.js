'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');

if (!common.hasCrypto)
  common.skip('missing crypto');

const child_process = require('child_process');
const http2 = require('http2');
const fs = require('fs');

const key = fixtures.readKey('agent8-key.pem', 'binary');
const cert = fixtures.readKey('agent8-cert.pem', 'binary');

const server = http2.createSecureServer({ key, cert }, (request, response) => {
  fs.createReadStream(process.execPath).pipe(response);
});

// This should be doable with a reproduction purely written in Node;
// that just requires somebody to take the time and actually do it.
server.listen(0, () => {
  const proc = child_process.spawn('h2load', [
    '-n', '1000',
    `https://localhost:${server.address().port}/`,
  ]);
  proc.on('error', (err) => {
    if (err.code === 'ENOENT')
      common.skip('no h2load');
  });
  proc.on('exit', () => server.close());
  setTimeout(() => proc.kill(2), 100);
});
