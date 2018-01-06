'use strict';
const common = require('../common');
const assert = require('assert');
const { createServer, get } = require('http');
const { spawn } = require('child_process');

if (process.argv[2] === 'child') {
  // sub-process
  const server = createServer(common.mustCall((_, res) => res.end('h')));
  server.listen(0, common.mustCall((s) => {
    const rr = get({ port: server.address().port }, common.mustCall(() => {
      // This bad input (0) should abort the parser and the process
      rr.parser.consume(0);
      // This line should be unreachable.
      assert.fail('this should be unreachable');
    }));
  }));
} else {
  // super-process
  const child = spawn(process.execPath, [__filename, 'child']);
  child.stdout.on('data', common.mustNotCall());

  let stderr = '';
  child.stderr.on('data', common.mustCallAtLeast((data) => {
    assert(Buffer.isBuffer(data));
    stderr += data.toString('utf8');
  }, 1));
  child.on('exit', common.mustCall((code, signal) => {
    assert(stderr.includes('failed'), `stderr: ${stderr}`);
    const didAbort = common.nodeProcessAborted(code, signal);
    assert(didAbort, `process did not abort, code:${code} signal:${signal}`);
  }));
}
