'use strict';
var common = require('../common');
var assert = require('assert');
var path = require('path');
var childProcess = require('child_process');
var fs = require('fs');

var scriptString = path.join(common.fixturesDir, 'print-chars.js');
var scriptBuffer = path.join(common.fixturesDir, 'print-chars-from-buffer.js');
var tmpFile = path.join(common.tmpDir, 'stdout.txt');

common.refreshTmpDir();

function test(size, useBuffer, cb) {
  var cmd = '"' + process.argv[0] + '"' +
            ' ' +
            '"' + (useBuffer ? scriptBuffer : scriptString) + '"' +
            ' ' +
            size +
            ' > ' +
            '"' + tmpFile + '"';

  try {
    fs.unlinkSync(tmpFile);
  } catch (e) {}

  console.log(size + ' chars to ' + tmpFile + '...');

  childProcess.exec(cmd, function(err) {
    if (err) throw err;

    console.log('done!');

    var stat = fs.statSync(tmpFile);

    console.log(tmpFile + ' has ' + stat.size + ' bytes');

    assert.equal(size, stat.size);
    fs.unlinkSync(tmpFile);

    cb();
  });
}

test(1024 * 1024, false, common.mustCall(function() {
  console.log('Done printing with string');
  test(1024 * 1024, true, common.mustCall(function() {
    console.log('Done printing with buffer');
  }));
}));
