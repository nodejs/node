'use strict';
const common = require('../common');
const assert = require('assert');
const path = require('path');
const fs = require('fs');
const spawn = require('child_process').spawn;
const tmpdir = require('../common/tmpdir');

let cat, grep, wc;

const KB = 1024;
const MB = KB * KB;


// Make sure process chaining allows desired data flow:
// check cat <file> | grep 'x' | wc -c === 1MB
// This helps to make sure no data is lost between pipes.

{
  tmpdir.refresh();
  const file = path.resolve(tmpdir.path, 'data.txt');
  const buf = Buffer.alloc(MB).fill('x');

  // Most OS commands that deal with data, attach special
  // meanings to new line - for example, line buffering.
  // So cut the buffer into lines at some points, forcing
  // data flow to be split in the stream.
  for (let i = 0; i < KB; i++)
    buf[i * KB] = 10;
  fs.writeFileSync(file, buf.toString());

  cat = spawn('cat', [file]);
  grep = spawn('grep', ['x'], { stdio: [cat.stdout, 'pipe', 'pipe'] });
  wc = spawn('wc', ['-c'], { stdio: [grep.stdout, 'pipe', 'pipe'] });

  [cat, grep, wc].forEach((child, index) => {
    child.stderr.on('data', (d) => {
      // Don't want to assert here, as we might miss error code info.
      console.error(`got unexpected data from child #${index}:\n${d}`);
    });
    child.on('exit', common.mustCall(function(code) {
      assert.strictEqual(code, 0);
    }));
  });

  wc.stdout.on('data', common.mustCall(function(data) {
    assert.strictEqual(data.toString().trim(), MB.toString());
  }));
}
