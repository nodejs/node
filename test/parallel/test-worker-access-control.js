// Flags: --experimental-worker
'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const child_process = require('child_process');
const { Worker } = require('worker_threads');

// Do not use isMainThread so that this test itself can be run inside a Worker.
if (!process.env.HAS_STARTED_WORKER) {
  process.env.HAS_STARTED_WORKER = 1;
  const w = new Worker(__filename, {
    accessControl: {
      fsWrite: false,
      childProcesses: false,
      loadAddons: false,
      createWorkers: false
    }
  });

  w.on('exit', common.mustCall((code) => {
    assert.strictEqual(code, 0);
  }));
} else {
  // We needed fs read access to load modules before.
  process.accessControl.apply({ fsRead: false });

  common.expectsError(() => { fs.readFileSync(__filename); }, {
    type: Error,
    message: 'Access to this API has been restricted',
    code: 'ERR_ACCESS_DENIED'
  });

  common.expectsError(() => { fs.writeFileSync(__filename, ''); }, {
    type: Error,
    message: 'Access to this API has been restricted',
    code: 'ERR_ACCESS_DENIED'
  });

  common.expectsError(() => { child_process.spawnSync('node'); }, {
    type: Error,
    message: 'Access to this API has been restricted',
    code: 'ERR_ACCESS_DENIED'
  });

  common.expectsError(() => { child_process.spawn('node'); }, {
    type: Error,
    message: 'Access to this API has been restricted',
    code: 'ERR_ACCESS_DENIED'
  });

  common.expectsError(() => { new Worker(__filename); }, {
    type: Error,
    message: 'Access to this API has been restricted',
    code: 'ERR_ACCESS_DENIED'
  });
}
