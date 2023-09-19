// Flags: --experimental-permission --allow-fs-read=* --allow-child-process
'use strict';

const common = require('../common');
common.skipIfWorker();

const assert = require('assert');
const { spawnSync } = require('child_process');

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
