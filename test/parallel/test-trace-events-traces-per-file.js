'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');
const path = require('path');

tmpdir.refresh();

// Needs to produce more than 1 << 17 traces.
const CODE = 'for (let i = 0; i < 20000; i++) (async ()=>{})()';

const spawn = common.mustCall((numTraces) => new Promise((resolve, reject) => {
  const proc = cp.spawn(process.execPath, [
    '--trace-events-enabled',
    '--trace-event-file-pattern',
    `trace-${numTraces}-\${pid}-\${rotation}.log`,
    '--trace-event-traces-per-file', numTraces,
    '-e', CODE
  ], { cwd: tmpdir.path, stdio: 'inherit' });

  proc.once('exit', common.mustCall(() => {
    const expectedFilename = path.join(tmpdir.path,
                                       `trace-${numTraces}-${proc.pid}-1.log`);
    assert(fs.existsSync(expectedFilename));
    fs.readFile(expectedFilename, common.mustCall((err, data) => {
      const traces = JSON.parse(data.toString()).traceEvents;
      resolve(traces.length);
    }));
  }));
}), 2);

// spawn(x) creates a new process where the value for
// --trace-event-traces-per-file is set to x.
Promise.all([spawn(1 << 16), spawn(1 << 17)])
  .then(([numTraces16, numTraces17]) => {
    assert.ok(numTraces16 >= (1 << 16));
    assert.ok(numTraces16 < (1 << 17));
    assert.ok(numTraces17 >= (1 << 17));
  });
