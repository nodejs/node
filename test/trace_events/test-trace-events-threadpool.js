'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
const { scheduler } = require('timers/promises');

const {
  defaultTraceFileName,
  readTraceEvents,
  checkTraceProcessor,
  traceCategory,
} = require('../common/trace_events');

if (!common.hasCrypto)
  common.skip('missing crypto');

checkTraceProcessor();

const { hkdf } = require('crypto');
const { deflate } = require('zlib');

if (process.env.isChild === '1') {
  hkdf('sha512', 'key', 'salt', 'info', 64, () => {});
  deflate('hello', () => {});
  scheduler.wait(10);
  return;
}

tmpdir.refresh();
const FILE_NAME = tmpdir.resolve(defaultTraceFileName);

cp.spawnSync(process.execPath,
             [
               '--trace-events-enabled',
               '--trace-event-categories',
               'node.threadpoolwork.sync,node.threadpoolwork.async',
               __filename,
             ],
             {
               cwd: tmpdir.path,
               env: {
                 ...process.env,
                 isChild: '1',
               },
             });

assert(fs.existsSync(FILE_NAME));
const traces = readTraceEvents(FILE_NAME);

assert(traces.length > 0);

let zlibCount = 0;
let cryptoCount = 0;

traces.forEach((item) => {
  if ([
    traceCategory('node.threadpoolwork.sync'),
    traceCategory('node.threadpoolwork.async'),
  ].includes(item.cat)) {
    if (item.name === 'zlib') {
      zlibCount++;
    } else if (item.name === 'crypto') {
      cryptoCount++;
    }
  }
});

// There are two types, each type has two async events and sync events at
// least. Perfetto records the sync begin/end pair as one complete event.
const expected = common.hasPerfetto ? 3 : 4;
assert.ok(zlibCount >= expected);
assert.ok(cryptoCount >= expected);
