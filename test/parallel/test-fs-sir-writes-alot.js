'use strict';
const common = require('../common');
const fs = require('fs');
const assert = require('assert');
const join = require('path').join;

const filename = join(common.tmpDir, 'out.txt');

common.refreshTmpDir();

const fd = fs.openSync(filename, 'w');

const line = 'aaaaaaaaaaaaaaaaaaaaaaaaaaaa\n';

const N = 10240;
let complete = 0;
for (let i = 0; i < N; i++) {
  // Create a new buffer for each write. Before the write is actually
  // executed by the thread pool, the buffer will be collected.
  const buffer = Buffer.from(line);
  fs.write(fd, buffer, 0, buffer.length, null, function(er, written) {
    complete++;
    if (complete === N) {
      fs.closeSync(fd);
      const s = fs.createReadStream(filename);
      s.on('data', testBuffer);
    }
  });
}

let bytesChecked = 0;

function testBuffer(b) {
  for (let i = 0; i < b.length; i++) {
    bytesChecked++;
    if (b[i] !== 'a'.charCodeAt(0) && b[i] !== '\n'.charCodeAt(0)) {
      throw new Error('invalid char ' + i + ',' + b[i]);
    }
  }
}

process.on('exit', function() {
  // Probably some of the writes are going to overlap, so we can't assume
  // that we get (N * line.length). Let's just make sure we've checked a
  // few...
  assert.ok(bytesChecked > 1000);
});
