'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const path = require('path');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');

const names = [
  'environment',
  'nodeStart',
  'v8Start',
  'loopStart',
  'loopExit',
  'bootstrapComplete',
  'thirdPartyMainStart',
  'thirdPartyMainEnd',
  'clusterSetupStart',
  'clusterSetupEnd',
  'moduleLoadStart',
  'moduleLoadEnd',
  'preloadModulesLoadStart',
  'preloadModulesLoadEnd'
];

if (process.argv[2] === 'child') {
  1 + 1;
} else {
  tmpdir.refresh();
  process.chdir(tmpdir.path);

  const proc = cp.fork(__filename,
                       [ 'child' ], {
                         execArgv: [
                           '--trace-event-categories',
                           'node.bootstrap'
                         ]
                       });

  proc.once('exit', common.mustCall(() => {
    const file = path.join(tmpdir.path, 'node_trace.1.log');

    assert(common.fileExists(file));
    fs.readFile(file, common.mustCall((err, data) => {
      const traces = JSON.parse(data.toString()).traceEvents;
      traces.forEach((trace) => {
        assert.strictEqual(trace.pid, proc.pid);
        assert(names.includes(trace.name));
      });
    }));
  }));
}
