// Flags: --experimental-permission --allow-fs-read=*
'use strict';

const common = require('../common');
common.skipIfWorker();

const assert = require('assert');
const fs = require('fs');
const path = require('path');

if (common.isWindows) {
  const { root } = path.parse(process.cwd());
  const abs = (p) => path.join(root, p);
  const denyList = [
    'tmp\\*',
    'example\\foo*',
    'example\\bar*',
    'folder\\*',
    'show',
    'slower',
    'slown',
    'home\\foo\\*',
  ].map(abs);
  assert.ok(process.permission.deny('fs.read', denyList));
  assert.ok(process.permission.has('fs.read', abs('slow')));
  assert.ok(process.permission.has('fs.read', abs('slows')));
  assert.ok(!process.permission.has('fs.read', abs('slown')));
  assert.ok(!process.permission.has('fs.read', abs('home\\foo')));
  assert.ok(!process.permission.has('fs.read', abs('home\\foo\\')));
  assert.ok(process.permission.has('fs.read', abs('home\\fo')));
} else {
  const denyList = [
    '/tmp/*',
    '/example/foo*',
    '/example/bar*',
    '/folder/*',
    '/show',
    '/slower',
    '/slown',
    '/home/foo/*',
  ];
  assert.ok(process.permission.deny('fs.read', denyList));
  assert.ok(process.permission.has('fs.read', '/slow'));
  assert.ok(process.permission.has('fs.read', '/slows'));
  assert.ok(!process.permission.has('fs.read', '/slown'));
  assert.ok(!process.permission.has('fs.read', '/home/foo'));
  assert.ok(!process.permission.has('fs.read', '/home/foo/'));
  assert.ok(process.permission.has('fs.read', '/home/fo'));
}

{
  assert.throws(() => {
    fs.readFile('/tmp/foo/file', () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
  }));
  // doesNotThrow
  fs.readFile('/test.txt', () => {});
  fs.readFile('/tmpd', () => {});
}

{
  assert.throws(() => {
    fs.readFile('/example/foo/file', () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
  }));
  assert.throws(() => {
    fs.readFile('/example/foo2/file', () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
  }));
  assert.throws(() => {
    fs.readFile('/example/foo2', () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
  }));

  // doesNotThrow
  fs.readFile('/example/fo/foo2.js', () => {});
  fs.readFile('/example/for', () => {});
}

{
  assert.throws(() => {
    fs.readFile('/example/bar/file', () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
  }));
  assert.throws(() => {
    fs.readFile('/example/bar2/file', () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
  }));
  assert.throws(() => {
    fs.readFile('/example/bar', () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
  }));

  // doesNotThrow
  fs.readFile('/example/ba/foo2.js', () => {});
}

{
  assert.throws(() => {
    fs.readFile('/folder/a/subfolder/b', () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
  }));
  assert.throws(() => {
    fs.readFile('/folder/a/subfolder/b/c.txt', () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
  }));
  assert.throws(() => {
    fs.readFile('/folder/a/foo2.js', () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
  }));
}
