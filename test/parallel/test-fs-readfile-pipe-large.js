'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path');

// simulate `cat readfile.js | node readfile.js`

// TODO: Have some way to make this work on windows.
if (process.platform === 'win32') {
  console.error('No /dev/stdin on windows.  Skipping test.');
  process.exit();
}

var fs = require('fs');

var filename = path.join(common.tmpDir, '/readfile_pipe_large_test.txt');
var dataExpected = new Array(1000000).join('a');
fs.writeFileSync(filename, dataExpected);

if (process.argv[2] === 'child') {
  fs.readFile('/dev/stdin', function(er, data) {
    if (er) throw er;
    process.stdout.write(data);
  });
  return;
}

var exec = require('child_process').exec;
var f = JSON.stringify(__filename);
var node = JSON.stringify(process.execPath);
var cmd = 'cat ' + filename + ' | ' + node + ' ' + f + ' child';
exec(cmd, { maxBuffer: 1000000 }, function(err, stdout, stderr) {
  if (err) console.error(err);
  assert(!err, 'it exits normally');
  assert(stdout === dataExpected, 'it reads the file and outputs it');
  assert(stderr === '', 'it does not write to stderr');
  console.log('ok');
});

process.on('exit', function() {
  fs.unlinkSync(filename);
});
