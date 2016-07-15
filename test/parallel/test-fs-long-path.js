'use strict';
var common = require('../common');
var fs = require('fs');
var path = require('path');

if (!common.isWindows) {
  common.skip('this test is Windows-specific.');
  return;
}

// make a path that will be at least 260 chars long.
var fileNameLen = Math.max(260 - common.tmpDir.length - 1, 1);
var fileName = path.join(common.tmpDir, new Array(fileNameLen + 1).join('x'));
var fullPath = path.resolve(fileName);

common.refreshTmpDir();

console.log({
  filenameLength: fileName.length,
  fullPathLength: fullPath.length
});

fs.writeFile(fullPath, 'ok', common.mustCall(function(err) {
  if (err) throw err;

  fs.stat(fullPath, common.mustCall(function(err, stats) {
    if (err) throw err;
  }));
}));

process.on('exit', function() {
  fs.unlinkSync(fullPath);
});
