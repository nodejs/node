'use strict';
var common = require('../common');
var fs = require('fs');
var path = require('path');
var assert = require('assert');

var successes = 0;

// make a path that will be at least 260 chars long.
var fileNameLen = Math.max(260 - common.tmpDir.length - 1, 1);
var fileName = path.join(common.tmpDir, new Array(fileNameLen + 1).join('x'));
var fullPath = path.resolve(fileName);

try {
  fs.unlinkSync(fullPath);
}
catch (e) {
  // Ignore.
}

console.log({
  filenameLength: fileName.length,
  fullPathLength: fullPath.length
});

fs.writeFile(fullPath, 'ok', function(err) {
  if (err) throw err;
  successes++;

  fs.stat(fullPath, function(err, stats) {
    if (err) throw err;
    successes++;
  });
});

process.on('exit', function() {
  fs.unlinkSync(fullPath);
  assert.equal(2, successes);
});
