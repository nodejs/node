'use strict';
var common = require('../common'),
    assert = require('assert'),
    fs = require('fs'),
    path = require('path');

var dir = path.resolve(common.fixturesDir),
    dirs = [];

// Make a long path.
for (var i = 0; i < 50; i++) {
  dir = dir + '/123456790';
  try {
    fs.mkdirSync(dir, '0777');
  } catch (e) {
    if (e.code == 'EEXIST') {
      // Ignore;
    } else {
      cleanup();
      throw e;
    }
  }
  dirs.push(dir);
}

// Test existsSync
var r = common.fileExists(dir);
if (r !== true) {
  cleanup();
  throw new Error('fs.accessSync returned false');
}

// Text exists
fs.access(dir, function(err) {
  cleanup();
  if (err) {
    throw new Error('fs.access reported false');
  }
});

// Remove all created directories
function cleanup() {
  for (var i = dirs.length - 1; i >= 0; i--) {
    fs.rmdirSync(dirs[i]);
  }
}
