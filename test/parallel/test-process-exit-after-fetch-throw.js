'use strict';
// Ref: https://github.com/nodejs/node/issues/56645
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const http = require('http');

if (process.argv[2] === 'child') {
  http
    .createServer((_, res) => {
      res.writeHead(302, { Location: '/' });
      res.end();
    })
    .listen(0, '127.0.0.1', async function() {
      try {
        await fetch(`http://127.0.0.1:${this.address().port}/`);
      } catch {
        // ignore
      }
      process.exit(0);
    });
} else {
  const child = cp.spawn(process.execPath, [__filename, 'child']);

  child.on(
    'close',
    common.mustCall((exitCode, signal) => {
      assert.strictEqual(exitCode, 0);
      assert.strictEqual(signal, null);
    }),
  );
}
