'use strict';
const common = require('../common');

// simulate `cat readfile.js | node readfile.js`

if (common.isWindows || common.isAIX)
  common.skip(`No /dev/stdin on ${process.platform}.`);

const assert = require('assert');
const path = require('path');
const fs = require('fs');

if (process.argv[2] === 'child') {
  fs.readFile('/dev/stdin', function(er, data) {
    assert.ifError(er);
    process.stdout.write(data);
  });
  return;
}

const tmpdir = require('../common/tmpdir');

const filename = path.join(tmpdir.path, '/readfile_pipe_large_test.txt');
const dataExpected = 'a'.repeat(999999);
tmpdir.refresh();
fs.writeFileSync(filename, dataExpected);

const exec = require('child_process').exec;
const f = JSON.stringify(__filename);
const node = JSON.stringify(process.execPath);
const cmd = `cat ${filename} | ${node} ${f} child`;
exec(cmd, { maxBuffer: 1000000 }, function(err, stdout, stderr) {
  assert.ifError(err);
  assert.strictEqual(
    stdout,
    dataExpected,
    `expect it reads the file and outputs 999999 'a' but got : ${stdout}`
  );
  assert.strictEqual(
    stderr,
    '',
    `expect that it does not write to stderr, but got : ${stderr}`
  );
  console.log('ok');
});

process.on('exit', function() {
  fs.unlinkSync(filename);
});
