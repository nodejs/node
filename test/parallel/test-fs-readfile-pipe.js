'use strict';
const common = require('../common');
const assert = require('assert');

// simulate `cat readfile.js | node readfile.js`

if (common.isWindows || common.isAix) {
  common.skip(`No /dev/stdin on ${process.platform}.`);
  return;
}

const fs = require('fs');

const dataExpected = fs.readFileSync(__filename, 'utf8');

if (process.argv[2] === 'child') {
  fs.readFile('/dev/stdin', function(er, data) {
    if (er) throw er;
    process.stdout.write(data);
  });
  return;
}

const exec = require('child_process').exec;
const f = JSON.stringify(__filename);
const node = JSON.stringify(process.execPath);
const cmd = `cat ${f} | ${node} ${f} child`;
exec(cmd, function(err, stdout, stderr) {
  if (err) console.error(err);
  assert(!err, 'it exits normally');
  assert.strictEqual(stdout, dataExpected, 'it reads the file and outputs it');
  assert.strictEqual(stderr, '', 'it does not write to stderr');
  console.log('ok');
});
