'use strict';
require('../common');
var path = require('path');
var assert = require('assert');
var spawn = require('child_process').spawn;

if (process.argv[2] !== 'child') {
  var child = spawn(process.execPath, [__filename, 'child'], {
    cwd: path.dirname(process.execPath)
  });

  var childArgv0 = '';
  child.stdout.on('data', function(chunk) {
    childArgv0 += chunk;
  });
  process.on('exit', function() {
    assert.equal(childArgv0, process.execPath);
  });
} else {
  process.stdout.write(process.argv[0]);
}
