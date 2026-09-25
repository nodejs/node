// Flags: --permission --allow-fs-read=* --allow-fs-write=*
'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');
const fs = require('fs');

{
  assert.ok(process.permission.has('fs.read'));
  assert.ok(process.permission.has('fs.read', __filename));
}

// Specific path drop is a no-op when * was granted
process.permission.drop('fs.read', '/tmp/some-path');
{
  assert.ok(process.permission.has('fs.read'));
  assert.ok(process.permission.has('fs.read', __filename));
}

process.permission.drop('fs.read');
{
  assert.ok(!process.permission.has('fs.read'));
  assert.ok(!process.permission.has('fs.read', __filename));
  assert.throws(() => {
    fs.readFileSync(__filename, 'utf8');
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
  }));
}

{
  assert.ok(process.permission.has('fs.write'));
}
