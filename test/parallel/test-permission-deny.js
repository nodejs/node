// Flags: --experimental-permission --allow-fs-read=* --allow-fs-write=*
'use strict';

const common = require('../common');
common.skipIfWorker();

const fs = require('fs');
const fsPromises = require('node:fs/promises');
const assert = require('assert');
const path = require('path');
const fixtures = require('../common/fixtures');

const protectedFolder = fixtures.path('permission', 'deny');
const protectedFile = fixtures.path('permission', 'deny', 'protected-file.md');
const regularFile = fixtures.path('permission', 'deny', 'regular-file.md');

// Assert has and deny exists
{
  assert.ok(typeof process.permission.has === 'function');
  assert.ok(typeof process.permission.deny === 'function');
}

// Guarantee the initial state when no flags
{
  assert.ok(process.permission.has('fs.read'));
  assert.ok(process.permission.has('fs.write'));

  assert.ok(process.permission.has('fs.read', protectedFile));
  assert.ok(process.permission.has('fs.read', regularFile));

  assert.ok(process.permission.has('fs.write', protectedFolder));
  assert.ok(process.permission.has('fs.write', regularFile));

  // doesNotThrow
  fs.readFileSync(protectedFile);
}

// Deny access to fs.read
{
  assert.ok(process.permission.deny('fs.read', [protectedFile]));
  assert.ok(process.permission.has('fs.read'));
  assert.ok(process.permission.has('fs.write'));

  assert.ok(process.permission.has('fs.read', regularFile));
  assert.ok(!process.permission.has('fs.read', protectedFile));

  assert.ok(process.permission.has('fs.write', protectedFolder));
  assert.ok(process.permission.has('fs.write', regularFile));

  assert.rejects(() => {
    return fsPromises.readFile(protectedFile);
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
  })).then(common.mustCall());

  // doesNotThrow
  fs.openSync(regularFile, 'w');
}

// Deny access to fs.write
{
  assert.ok(process.permission.deny('fs.write', [protectedFolder]));
  assert.ok(process.permission.has('fs.read'));
  assert.ok(process.permission.has('fs.write'));

  assert.ok(!process.permission.has('fs.read', protectedFile));
  assert.ok(process.permission.has('fs.read', regularFile));

  assert.ok(!process.permission.has('fs.write', protectedFolder));
  assert.ok(!process.permission.has('fs.write', regularFile));

  assert.rejects(() => {
    return fsPromises
     .writeFile(path.join(protectedFolder, 'new-file'), 'data');
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
  })).then(common.mustCall());

  assert.throws(() => {
    fs.openSync(regularFile, 'w');
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
  }));
}

// Should not crash if wrong parameter is provided
{
  // Array is expected as second parameter
  assert.throws(() => {
    process.permission.deny('fs.read', protectedFolder);
  }, common.expectsError({
    code: 'ERR_INVALID_ARG_TYPE',
  }));
}
