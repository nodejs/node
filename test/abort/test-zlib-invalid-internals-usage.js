'use strict';
require('../common');
const assert = require('assert');
const os = require('os');
const cp = require('child_process');

if (process.argv[2] === 'child') {
  // This is the heart of the test.
  new (process.binding('zlib').Zlib)(0).init(1, 2, 3, 4, 5);
} else {
  const child = cp.spawnSync(`${process.execPath}`, [`${__filename}`, 'child']);

  assert.strictEqual(child.stdout.toString(), '');
  assert.ok(child.stderr.includes(
    'WARNING: You are likely using a version of node-tar or npm that ' +
    `is incompatible with this version of Node.js.${os.EOL}` +
    'Please use either the version of npm that is bundled with Node.js, or ' +
    'a version of npm (> 5.5.1 or < 5.4.0) or node-tar (> 4.0.1) that is ' +
    `compatible with Node.js 9 and above.${os.EOL}`));
}
