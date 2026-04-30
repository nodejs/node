// Flags: --permission --allow-worker --allow-fs-read=*
'use strict';

const common = require('../common');
const { isMainThread, Worker } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');

{
  assert.ok(process.permission.has('worker'));
}

process.permission.drop('worker');
{
  assert.ok(!process.permission.has('worker'));
  assert.throws(() => {
    new Worker('console.log("hello")', { eval: true });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'WorkerThreads',
  }));
}
