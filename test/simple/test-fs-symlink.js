require("../common");
var path = require('path');
var fs = require('fs');
var completed = 0;

// test creating and reading symbolic link
var linkData = "../../cycles/root.js";
var linkPath = path.join(fixturesDir, "nested-index", 'one', 'symlink1.js');
try {fs.unlinkSync(linkPath);}catch(e){}
fs.symlink(linkData, linkPath, function(err){
  if (err) throw err;
  puts('symlink done');
  // todo: fs.lstat?
  fs.readlink(linkPath, function(err, destination) {
    if (err) throw err;
    assert.equal(destination, linkData);
    completed++;
  })
});

// test creating and reading hard link
var srcPath = path.join(fixturesDir, "cycles", 'root.js');
var dstPath = path.join(fixturesDir, "nested-index", 'one', 'link1.js');
try {fs.unlinkSync(dstPath);}catch(e){}
fs.link(srcPath, dstPath, function(err){
  if (err) throw err;
  puts('hard link done');
  var srcContent = fs.readFileSync(srcPath);
  var dstContent = fs.readFileSync(dstPath);
  assert.equal(srcContent, dstContent);
  completed++;
});

process.addListener("exit", function () {
  try {fs.unlinkSync(linkPath);}catch(e){}
  try {fs.unlinkSync(dstPath);}catch(e){}
  assert.equal(completed, 2);
});

