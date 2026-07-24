'use strict';

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

if (process.argv[2] === 'child') {
  const { createServer } = require('http');

  const server = createServer({
    connectionsCheckingInterval: 1,
  }, (_req, res) => {
    res.end('ok');
  });

  server.headersTimeout = 'im-not-a-number';

  server.listen(0, '127.0.0.1', () => {
    setTimeout(() => {
      server.close(() => process.exit(0));
    }, 50);
  });
} else {
  // Run the repro in a child so the native crash is observable without failing the whole run
  const { signal, status, stderr } = spawnSync(
    process.execPath,
    [__filename, 'child'],
    { encoding: 'utf8' },
  );

  assert.strictEqual(signal, null);
  assert.strictEqual(status, 0, stderr);
}
