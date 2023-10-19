'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');
const { scheduler } = require('timers/promises');

if (!common.hasCrypto)
  common.skip('missing crypto');

const { hkdf } = require('crypto');
const { deflate } = require('zlib');

if (process.env.isChild === '1') {
  hkdf('sha512', 'key', 'salt', 'info', 64, () => {});
  deflate('hello', () => {});
  scheduler.wait(10);
  return;
}

tmpdir.refresh();
const FILE_NAME = tmpdir.resolve('node_trace.1.log');

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
const data = fs.readFileSync(FILE_NAME);
const traces = JSON.parse(data.toString()).traceEvents;

assert(traces.length > 0);

let zlibCount = 0;
let cryptoCount = 0;

traces.forEach((item) => {
  if ([
    'node,node.threadpoolwork,node.threadpoolwork.sync',
    'node,node.threadpoolwork,node.threadpoolwork.async',
  ].includes(item.cat)) {
    if (item.name === 'zlib') {
      zlibCount++;
    } else if (item.name === 'crypto') {
      cryptoCount++;
    }
  }
});

// There are two types, each type has two async events and sync events at least
assert.ok(zlibCount >= 4);
assert.ok(cryptoCount >= 4);
