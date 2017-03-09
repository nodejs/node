'use strict';
const common = require('../common');
const assert = require('assert');

const spawn = require('child_process').spawn;

const SIZE = 1000 * 1024;
const N = 40;
let finished = false;

function doSpawn(i) {
  const child = spawn('python', ['-c', 'print ' + SIZE + ' * "C"']);
  let count = 0;

  child.stdout.setEncoding('ascii');
  child.stdout.on('data', (chunk) => {
    count += chunk.length;
  });

  child.stderr.on('data', (chunk) => {
    console.log('stderr: ' + chunk);
  });

  child.on('close', () => {
    // + 1 for \n or + 2 for \r\n on Windows
    assert.strictEqual(SIZE + (common.isWindows ? 2 : 1), count);
    if (i < N) {
      doSpawn(i + 1);
    } else {
      finished = true;
    }
  });
}

doSpawn(0);

process.on('exit', () => {
  assert.ok(finished);
});
