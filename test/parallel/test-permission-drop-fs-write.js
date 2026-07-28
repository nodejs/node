// Flags: --permission --allow-fs-read=* --allow-fs-write=*
'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');

{
  assert.ok(process.permission.has('fs.write'));
}

// Specific path drop is a no-op when * was granted
process.permission.drop('fs.write', '/tmp/some-path');
{
  assert.ok(process.permission.has('fs.write'));
}

process.permission.drop('fs.write');
{
  assert.ok(!process.permission.has('fs.write'));
}

{
  assert.ok(process.permission.has('fs.read'));
  assert.ok(process.permission.has('fs.read', __filename));
}
