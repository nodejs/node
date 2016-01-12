'use strict';
var assert = require('assert');
var fs = require('fs');
var path = require('path');
var common = require('../common');
var msg = {test: 'this'};
var nodeCopyPath = path.join(common.tmpDir, 'node-copy.exe');
var chakracoreCopyPath = path.join(common.tmpDir, 'chakracore.dll');
var exePaths = [
    {srcPath : process.execPath,
     destPath : nodeCopyPath}];
if (common.isChakraEngine) {
  // chakra needs chakracore.dll as well
  exePaths.push(
      {srcPath : process.execPath.replace('node.exe', 'chakracore.dll'),
       destPath : chakracoreCopyPath});
}

if (process.env.FORK) {
  assert(process.send);
  assert.equal(process.argv[0], nodeCopyPath);
  process.send(msg);
  process.exit();
}
else {
  common.refreshTmpDir();
  try {
    exePaths.forEach(function(value) {
      fs.unlinkSync(value.destPath);
    });
  }
  catch (e) {
    if (e.code !== 'ENOENT') throw e;
  }

  exePaths.forEach(function(value) {
    fs.writeFileSync(value.destPath, fs.readFileSync(value.srcPath));
    fs.chmodSync(value.destPath, '0755');
  });

  // slow but simple
  var envCopy = JSON.parse(JSON.stringify(process.env));
  envCopy.FORK = 'true';
  var child = require('child_process').fork(__filename, {
    execPath: nodeCopyPath,
    env: envCopy
  });
  child.on('message', common.mustCall(function(recv) {
    assert.deepEqual(msg, recv);
  }));
  child.on('exit', common.mustCall(function(code) {
    exePaths.forEach(function(value) {
      fs.unlinkSync(value.destPath);
    });
    assert.equal(code, 0);
  }));
}
