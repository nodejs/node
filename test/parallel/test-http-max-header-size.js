'use strict';

const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const http = require('http');

assert.strictEqual(http.maxHeaderSize, 8 * 1024);
const child = spawnSync(process.execPath, ['--max-http-header-size=10', '-p',
                                           'http.maxHeaderSize']);
assert.strictEqual(+child.stdout.toString().trim(), 10);

{
  const server = http.createServer(common.mustNotCall());
  server.listen(0, common.mustCall(() => {
    http.get({
      port: server.address().port,
      headers: { foo: 'x'.repeat(http.maxHeaderSize + 1) }
    }, common.mustNotCall()).once('error', common.mustCall((err) => {
      assert.strictEqual(err.code, 'ECONNRESET');
      server.close();
    }));
  }));
}
