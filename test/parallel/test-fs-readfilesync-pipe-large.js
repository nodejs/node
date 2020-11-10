'use strict';
const common = require('../common');

// Simulate `cat readfile.js | node readfile.js`

if (common.isWindows || common.isAIX)
  common.skip(`No /dev/stdin on ${process.platform}.`);

const assert = require('assert');
const path = require('path');
const fs = require('fs');

if (process.argv[2] === 'child') {
  process.stdout.write(fs.readFileSync('/dev/stdin', 'utf8'));
  return;
}

const tmpdir = require('../common/tmpdir');

const filename = path.join(tmpdir.path, '/readfilesync_pipe_large_test.txt');
const dataExpected = 'a'.repeat(999999);
tmpdir.refresh();
fs.writeFileSync(filename, dataExpected);

const exec = require('child_process').exec;
const f = JSON.stringify(__filename);
const node = JSON.stringify(process.execPath);
const cmd = `cat ${filename} | ${node} ${f} child`;
exec(
  cmd,
  { maxBuffer: 1000000 },
  common.mustCall(function(err, stdout, stderr) {
    assert.ifError(err);
    assert.strictEqual(stdout, dataExpected);
    assert.strictEqual(stderr, '');
    console.log('ok');
  })
);
