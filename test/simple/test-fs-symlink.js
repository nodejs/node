var common = require('../common');
var assert = require('assert');
var path = require('path');
var fs = require('fs');
var completed = 0;

// test creating and reading symbolic link
var linkData = path.join(common.fixturesDir, '/cycles/root.js');
var linkPath = path.join(common.tmpDir, 'symlink1.js');
fs.symlink(linkData, linkPath, function(err) {
  if (err) throw err;
  console.log('symlink done');
  // todo: fs.lstat?
  fs.readlink(linkPath, function(err, destination) {
    if (err) throw err;
    assert.equal(destination, linkData);
    completed++;
  });
});

// test creating and reading hard link
var srcPath = path.join(common.fixturesDir, 'cycles', 'root.js');
var dstPath = path.join(common.tmpDir, 'link1.js');
fs.link(srcPath, dstPath, function(err) {
  if (err) throw err;
  console.log('hard link done');
  var srcContent = fs.readFileSync(srcPath, 'utf8');
  var dstContent = fs.readFileSync(dstPath, 'utf8');
  assert.equal(srcContent, dstContent);
  completed++;
});

process.addListener('exit', function() {
  assert.equal(completed, 2);
});

