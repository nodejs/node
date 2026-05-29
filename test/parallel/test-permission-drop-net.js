// Flags: --permission --allow-net --allow-fs-read=*
'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');

{
  assert.ok(process.permission.has('net'));
}

process.permission.drop('net');
{
  assert.ok(!process.permission.has('net'));
}
