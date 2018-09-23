'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const path = require('path');
const fs = require('fs');
const tmpdir = require('../common/tmpdir');

if (!common.isMainThread)
  common.skip('process.chdir is not available in Workers');

const names = [
  'ContextifyScript::New',
  'RunInThisContext',
  'RunInContext'
];

if (process.argv[2] === 'child') {
  const vm = require('vm');
  vm.runInNewContext('1 + 1');
} else {
  tmpdir.refresh();
  process.chdir(tmpdir.path);

  const proc = cp.fork(__filename,
                       [ 'child' ], {
                         execArgv: [
                           '--trace-event-categories',
                           'node.vm.script'
                         ]
                       });

  proc.once('exit', common.mustCall(() => {
    const file = path.join(tmpdir.path, 'node_trace.1.log');

    assert(fs.existsSync(file));
    fs.readFile(file, common.mustCall((err, data) => {
      const traces = JSON.parse(data.toString()).traceEvents
        .filter((trace) => trace.cat !== '__metadata');
      traces.forEach((trace) => {
        assert.strictEqual(trace.pid, proc.pid);
        assert(names.includes(trace.name));
      });
    }));
  }));
}
