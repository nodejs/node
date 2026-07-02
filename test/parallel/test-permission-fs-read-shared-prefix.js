'use strict';

require('../common');

const { spawnSync } = require('child_process');
const assert = require('assert');
const path = require('path');

// Granting several paths that share a common prefix must not implicitly
// grant access to the shared prefix itself. Inserting the third sibling used
// to mark the shared split node as an end node in the permission radix tree.
{
  const prefix = path.resolve('./shared-prefix-dir/app');
  const f1 = `${prefix}1.log`;
  const f2 = `${prefix}2.log`;
  const f3 = `${prefix}3.log`;
  const { status, stdout } = spawnSync(
    process.execPath,
    [
      '--permission',
      `--allow-fs-read=${f1}`,
      `--allow-fs-read=${f2}`,
      `--allow-fs-read=${f3}`,
      '-e',
      `console.log(process.permission.has("fs.read", ${JSON.stringify(prefix)}));
      console.log(process.permission.has("fs.read", ${JSON.stringify(f1)}));
      console.log(process.permission.has("fs.read", ${JSON.stringify(f2)}));
      console.log(process.permission.has("fs.read", ${JSON.stringify(f3)}));`,
    ]
  );
  const [sharedPrefix, allowed1, allowed2, allowed3] = stdout.toString().split('\n');
  assert.strictEqual(sharedPrefix, 'false');
  assert.strictEqual(allowed1, 'true');
  assert.strictEqual(allowed2, 'true');
  assert.strictEqual(allowed3, 'true');
  assert.strictEqual(status, 0);
}
