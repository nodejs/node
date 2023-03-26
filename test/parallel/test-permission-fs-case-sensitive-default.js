// Flags: --experimental-permission --allow-fs-read=*
'use strict';

const common = require('../common');
common.skipIfWorker();

const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const fixtures = require('../common/fixtures');

const protectedFile = fixtures.path('permission', 'deny', 'protected-file.md');
const protectedFileCapsLetters = fixtures.path('permission', 'deny', 'PrOtEcTeD-File.MD');

{
  assert.ok(process.permission.deny('fs.read', [protectedFile]));
}

{
  // Guarantee the initial protection
  assert.throws(() => {
    fs.readFile(protectedFile, () => {});
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(protectedFile),
  }));
}

// case-sensitive = true
if (common.isLinux) {
  // doesNotThrow
  fs.readFile(protectedFile.toUpperCase(), (err) => {
    assert.ifError(err);
  });

  // doesNotThrow
  fs.readFile(protectedFileCapsLetters, (err) => {
    assert.ifError(err);
  });
} else {
  assert.throws(() => {
    fs.readFile(protectedFile.toUpperCase(), (err) => {
      assert.ifError(err);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(protectedFile.toUpperCase()),
  }));

  assert.throws(() => {
    fs.readFile(protectedFileCapsLetters, (err) => {
      assert.ifError(err);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(protectedFileCapsLetters),
  }));
}
