'use strict';

const {
  hasCrypto,
  mustCall,
  skip
} = require('../common');
if (!hasCrypto)
  skip('missing crypto');

const {
  strictEqual
} = require('assert');
const {
  createServer,
  connect
} = require('http2');
const {
  spawnSync
} = require('child_process');

// Validate that there is no unexpected output when
// using http2
if (process.argv[2] !== 'child') {
  const {
    stdout, stderr, status
  } = spawnSync(process.execPath, [__filename, 'child'], { encoding: 'utf8' });
  strictEqual(stderr, '');
  strictEqual(stdout, '');
  strictEqual(status, 0);
} else {
  const server = createServer();
  server.listen(0, mustCall(() => {
    const client = connect(`http://localhost:${server.address().port}`);
    client.on('connect', mustCall(() => {
      client.close();
      server.close();
    }));
  }));
}
