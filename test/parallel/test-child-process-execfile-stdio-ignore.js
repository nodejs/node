'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

if (process.argv[2] === 'child') {
  process.stdout.write('this should be ignored');
  process.stderr.write('this should not be ignored');

} else {
  cp.execFile(process.execPath,
              [__filename, 'child'],
              { stdio: ['pipe', 'ignore', 'pipe'] },
              common.mustCall((err, stdout, stderr) => {
                assert.strictEqual(stderr, 'this should not be ignored');
                assert.strictEqual(stdout, '');
              }));
}
