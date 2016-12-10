'use strict';
var common = require('../common');
var fs = require('fs');
var assert = require('assert');
var join = require('path').join;

var filename = join(common.tmpDir, 'out.txt');

common.refreshTmpDir();

var fd = fs.openSync(filename, 'w');

var line = 'aaaaaaaaaaaaaaaaaaaaaaaaaaaa\n';

var N = 10240, complete = 0;
for (var i = 0; i < N; i++) {
  // Create a new buffer for each write. Before the write is actually
  // executed by the thread pool, the buffer will be collected.
  var buffer = Buffer.from(line);
  fs.write(fd, buffer, 0, buffer.length, null, function(er, written) {
    complete++;
    if (complete === N) {
      fs.closeSync(fd);
      var s = fs.createReadStream(filename);
      s.on('data', testBuffer);
    }
  });
}

var bytesChecked = 0;

function testBuffer(b) {
  for (var i = 0; i < b.length; i++) {
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
