'use strict';
var common = require('../common');
var fs = require('fs');
var path = require('path');
var assert = require('assert');

// make a path that is more than 260 chars long.
var fileNameLen = Math.max(261 - common.tmpDir.length - 1, 1);
var fileName = path.join(common.tmpDir, new Array(fileNameLen + 1).join('x'));
var fullPath = path.resolve(fileName);

common.refreshTmpDir();
fs.writeFileSync(fullPath, 'module.exports = 42;');

assert.equal(require(fullPath), 42);

fs.unlinkSync(fullPath);
