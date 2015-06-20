'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path');

var fs = require('fs');

var pipe = common.PIPE;
var dataExpected = new Array(1000000).join('a');
common.refreshTmpDir();
fs.writeFileSync(pipe, dataExpected);

fs.readFile(pipe, function(er, data) {
  if (er) throw er;
  process.stdout.write(data);
  assert(data.toString() === dataExpected, 'data read is same as data written');
});

process.on('exit', function() {
  fs.unlinkSync(pipe);
});
