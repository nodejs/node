'use strict';
const common = require('../common');
const fs = require('fs');
const assert = require('assert');
const path = require('path');
const file = path.join(common.tmpDir, '/read_stream_fd_test.txt');
const input = 'hello world';
let output = '';

common.refreshTmpDir();
fs.writeFileSync(file, input);
const fd = fs.openSync(file, 'r');

const stream = fs.createReadStream(null, { fd: fd, encoding: 'utf8' });
stream.on('data', function(data) {
  output += data;
});

process.on('exit', function() {
  fs.unlinkSync(file);
  assert.equal(output, input);
});
