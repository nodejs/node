'use strict'

const common = require('../../common');

const assert = require('assert');
const fs = require('fs');

{
  assert.throws(() => {
    fs.readFile('/test.txt', () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
  }));
  // doesNotThrow
  fs.readFile('/tmp/foo/file', () => {});
}

{
  // doesNotThrow
  fs.readFile('/example/foo/file', () => {});
  fs.readFile('/example/foo2/file', () => {});
  fs.readFile('/example/foo2', () => {});

  assert.throws(() => {
    fs.readFile('/example/fo/foo2.js', () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
  }));
  assert.throws(() => {
    fs.readFile('/example/for', () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
  }));
}

{
  // doesNotThrow
  fs.readFile('/example/bar/file', () => {});
  fs.readFile('/example/bar2/file', () => {});
  fs.readFile('/example/bar', () => {});

  assert.throws(() => {
    fs.readFile('/example/ba/foo2.js', () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
  }));
}

{
  fs.readFile('/folder/a/subfolder/b', () => {});
  fs.readFile('/folder/a/subfolder/b/c.txt', () => {});
  fs.readFile('/folder/a/foo2.js', () => {});
}
