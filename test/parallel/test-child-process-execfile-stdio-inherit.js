'use strict';
require('../common');
const assert = require('assert');
const execFile = require('child_process').execFile;

if (process.argv[2] === 'parent')
  parent();
else
  grandparent();

function grandparent() {
  const input = 'asdfasdf';
  const child = execFile(process.execPath, [__filename, 'parent'], (err, stdout, stderr) => {
    assert.strictEqual(stdout.trim(), input.trim());
  });
  child.stderr.pipe(process.stderr);

  child.stdin.end(input);
}

function parent() {
  // Should not immediately exit.
  execFile('cat', [], { stdio: 'inherit' });
}
