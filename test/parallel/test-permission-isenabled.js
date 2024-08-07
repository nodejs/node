// Flags: --experimental-permission --allow-fs-read=*
'use strict';

require('../common');

const assert = require('node:assert');
const { spawnSync } = require('node:child_process');

{
  assert.ok(typeof process.permission.isEnabled === 'function');
  assert.ok(process.permission.isEnabled());
}

{
  const { status } = spawnSync(
    process.execPath,
    [
      '-e',
      `
        console.log(process.permission.isEnabled());
      `,
    ],
  );
  // permission.isEnabled() should not be exposed when --no-experimental-permission
  assert.strictEqual(status, 1);
}
