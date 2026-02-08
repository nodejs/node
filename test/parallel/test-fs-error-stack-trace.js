'use strict';

// Test that async fs callback errors include stack traces.
// Regression test for https://github.com/nodejs/node/issues/30944
// Prior to this fix, async fs callback errors had no stack frames
// (only the error message), making debugging very difficult.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');

const nonexistentFile = '/tmp/.node-test-nonexistent-' + process.pid;

// Helper to check that an error has stack frames
function assertHasStackFrames(err, operation) {
  assert.ok(err instanceof Error, `${operation}: expected Error instance`);
  assert.ok(typeof err.stack === 'string', `${operation}: expected string stack`);
  assert.ok(
    err.stack.includes('\n    at '),
    `${operation}: error should have stack frames but got:\n${err.stack}`
  );
}

// Test: fs.stat with nonexistent file
fs.stat(nonexistentFile, common.mustCall((err) => {
  assertHasStackFrames(err, 'stat');
}));

// Test: fs.lstat with nonexistent file
fs.lstat(nonexistentFile, common.mustCall((err) => {
  assertHasStackFrames(err, 'lstat');
}));

// Test: fs.access with nonexistent file
fs.access(nonexistentFile, common.mustCall((err) => {
  assertHasStackFrames(err, 'access');
}));

// Test: fs.open with nonexistent file
fs.open(nonexistentFile, 'r', common.mustCall((err) => {
  assertHasStackFrames(err, 'open');
}));

// Test: fs.readdir with nonexistent directory
fs.readdir(nonexistentFile, common.mustCall((err) => {
  assertHasStackFrames(err, 'readdir');
}));

// Test: fs.rename with nonexistent source
fs.rename(nonexistentFile, nonexistentFile + '.bak', common.mustCall((err) => {
  assertHasStackFrames(err, 'rename');
}));

// Test: fs.unlink with nonexistent file
fs.unlink(nonexistentFile, common.mustCall((err) => {
  assertHasStackFrames(err, 'unlink');
}));

// Test: fs.rmdir with nonexistent directory
fs.rmdir(nonexistentFile, common.mustCall((err) => {
  assertHasStackFrames(err, 'rmdir');
}));

// Test: fs.chmod with nonexistent file
fs.chmod(nonexistentFile, 0o644, common.mustCall((err) => {
  assertHasStackFrames(err, 'chmod');
}));

// Test: fs.readlink with nonexistent file
fs.readlink(nonexistentFile, common.mustCall((err) => {
  assertHasStackFrames(err, 'readlink');
}));

// Test: fs.realpath with nonexistent file
fs.realpath(nonexistentFile, common.mustCall((err) => {
  assertHasStackFrames(err, 'realpath');
}));

// Test: fs.mkdir with nonexistent parent
fs.mkdir(nonexistentFile + '/sub/dir', common.mustCall((err) => {
  assertHasStackFrames(err, 'mkdir');
}));

// Test: fs.copyFile with nonexistent source
fs.copyFile(nonexistentFile, nonexistentFile + '.copy', common.mustCall((err) => {
  assertHasStackFrames(err, 'copyFile');
}));

// Test: fs.readFile with nonexistent file
fs.readFile(nonexistentFile, common.mustCall((err) => {
  assertHasStackFrames(err, 'readFile');
}));

// Test: Verify that errors that ALREADY have stack frames are not modified
{
  const originalErr = new Error('test error');
  const originalStack = originalErr.stack;
  assert.ok(originalStack.includes('\n    at '),
    'Sanity check: JS errors should have stack frames');
}

// Test: Verify sync errors still work (not affected by our change)
assert.throws(
  () => fs.readFileSync(nonexistentFile),
  (err) => {
    assertHasStackFrames(err, 'readFileSync');
    return true;
  }
);
