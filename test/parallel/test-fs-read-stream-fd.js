'use strict';
var fs = require('fs');
var assert = require('assert');
var path = require('path');

var common = require('../common');

var file = path.join(common.tmpDir, '/read_stream_fd_test.txt');
var input = 'hello world';
var output = '';
var fd, stream;

common.refreshTmpDir();
fs.writeFileSync(file, input);
fd = fs.openSync(file, 'r');

stream = fs.createReadStream(null, { fd: fd, encoding: 'utf8' });
stream.on('data', function(data) {
  output += data;
});

process.on('exit', function() {
  fs.unlinkSync(file);
  assert.equal(output, input);
});
