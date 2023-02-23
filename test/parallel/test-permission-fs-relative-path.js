// Flags: --experimental-permission --allow-fs-read=*
'use strict';

const common = require('../common');
common.skipIfWorker();

const assert = require('assert');
const fixtures = require('../common/fixtures');
const { spawnSync } = require('child_process');

const protectedFile = fixtures.path('permission', 'deny', 'protected-file.md');
const relativeProtectedFile = './test/fixtures/permission/deny/protected-file.md';

// Note: for relative path on fs.* calls, check test-permission-deny-fs-[read/write].js files

{
  // permission.deny relative path should work
  assert.ok(process.permission.has('fs.read', protectedFile));
  assert.ok(process.permission.deny('fs.read', [relativeProtectedFile]));
  assert.ok(!process.permission.has('fs.read', protectedFile));
}

{
  // permission.has relative path should work
  assert.ok(!process.permission.has('fs.read', relativeProtectedFile));
}

{
  // Relative path as CLI args are NOT supported yet
  const { status, stdout } = spawnSync(
    process.execPath,
    [
      '--experimental-permission',
      '--allow-fs-read', '*',
      '--allow-fs-write', '../fixtures/permission/deny/regular-file.md',
      '-e',
      `
      const path = require("path");
      const absolutePath = path.resolve("../fixtures/permission/deny/regular-file.md");
      console.log(process.permission.has("fs.write", absolutePath));
       `,
    ]
  );

  const [fsWrite] = stdout.toString().split('\n');
  assert.strictEqual(fsWrite, 'false');
  assert.strictEqual(status, 0);
}
