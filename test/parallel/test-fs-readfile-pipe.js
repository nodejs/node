'use strict';
var common = require('../common');
var assert = require('assert');

// simulate `cat readfile.js | node readfile.js`

if (common.isWindows || common.isAix) {
  console.log(`1..0 # Skipped: No /dev/stdin on ${process.platform}.`);
  return;
}

var fs = require('fs');

var dataExpected = fs.readFileSync(__filename, 'utf8');

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
var cmd = 'cat ' + f + ' | ' + node + ' ' + f + ' child';
exec(cmd, function(err, stdout, stderr) {
  if (err) console.error(err);
  assert(!err, 'it exits normally');
  assert(stdout === dataExpected, 'it reads the file and outputs it');
  assert(stderr === '', 'it does not write to stderr');
  console.log('ok');
});
