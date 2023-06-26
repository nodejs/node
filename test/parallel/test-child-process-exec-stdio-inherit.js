'use strict';
require('../common');
const assert = require('assert');
const exec = require('child_process').exec;

if (process.argv[2] === 'parent')
  parent();
else
  grandparent();

function grandparent() {
  const cmd = `"${process.execPath}" "${__filename}" parent`;
  const input = 'asdfasdf';
  const child = exec(cmd, (err, stdout, stderr) => {
    assert.strictEqual(stdout.trim(), input.trim());
  });
  child.stderr.pipe(process.stderr);

  child.stdin.end(input);
}

function parent() {
  // Should not immediately exit.
  exec('cat', { stdio: 'inherit' });
}
