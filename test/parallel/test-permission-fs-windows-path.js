// Flags: --experimental-permission --allow-fs-read=* --allow-fs-write=*
'use strict';

const common = require('../common');
common.skipIfWorker();

const assert = require('assert');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

if (!common.isWindows) {
  common.skip('windows test');
}

const protectedFolder = fixtures.path('permission', 'deny', 'protected-folder');

{
  assert.ok(process.permission.has('fs.write', protectedFolder));
  assert.ok(process.permission.deny('fs.write', [protectedFolder]));
  assert.ok(!process.permission.has('fs.write', protectedFolder));
}

{
  assert.throws(() => {
    fs.openSync(path.join(protectedFolder, 'protected-file.md'), 'w');
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(path.join(protectedFolder, 'protected-file.md')),
  }));

  assert.rejects(() => {
    return new Promise((_resolve, reject) => {
      const stream = fs.createWriteStream(path.join(protectedFolder, 'protected-file.md'));
      stream.on('error', reject);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(path.join(protectedFolder, 'protected-file.md')),
  })).then(common.mustCall());
}

{
  const { stdout } = spawnSync(process.execPath, [
    '--experimental-permission', '--allow-fs-write', 'C:\\\\', '-e',
    'console.log(process.permission.has("fs.write", "C:\\\\"))',
  ]);
  assert.strictEqual(stdout.toString(), 'true\n');
}

{
  assert.ok(process.permission.has('fs.write', 'C:\\home'));
  assert.ok(process.permission.deny('fs.write', ['C:\\home']));
  assert.ok(!process.permission.has('fs.write', 'C:\\home'));
}

{
  assert.ok(process.permission.has('fs.write', '\\\\?\\C:\\'));
  assert.ok(process.permission.deny('fs.write', ['\\\\?\\C:\\']));
  // UNC aren't supported so far
  assert.ok(process.permission.has('fs.write', 'C:/'));
  assert.ok(process.permission.has('fs.write', '\\\\?\\C:\\'));
}
