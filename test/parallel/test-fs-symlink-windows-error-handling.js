// Test for Windows symlink error handling when stat() fails
'use strict';

const common = require('../common');

// This test is specifically for Windows symlink behavior
if (!common.isWindows) {
  common.skip('Windows-specific test');
}

if (!common.canCreateSymLink()) {
  common.skip('insufficient privileges');
}

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

// Test that symlink creation doesn't crash when stat() fails on absoluteTarget
// This tests the try-catch error handling added around stat() call
const target = './nonexistent-target';
const linkPath = tmpdir.resolve('test-symlink');

// Create a symlink with a relative target that would cause stat() to fail
// when resolving the absolute target path
fs.symlink(target, linkPath, common.mustSucceed(() => {
  // Verify the symlink was created successfully despite stat() error
  fs.lstat(linkPath, common.mustSucceed((stats) => {
    assert(stats.isSymbolicLink());
  }));
  
  fs.readlink(linkPath, common.mustSucceed((destination) => {
    assert.strictEqual(destination, target);
  }));
}));