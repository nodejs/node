'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');

if (!common.isMainThread)
  common.skip('process.chdir is not available in Workers');

const tmpdir = require('../common/tmpdir');

const FILE_NAME = 'node_trace.1.log';

tmpdir.refresh();
process.chdir(tmpdir.path);

const proc = cp.spawn(process.execPath,
                      [ '--trace-events-enabled',
                        '-e', 'process.exit()' ]);

proc.once('exit', common.mustCall(() => {
  assert(fs.existsSync(FILE_NAME));
  fs.readFile(FILE_NAME, common.mustCall((err, data) => {
    const traces = JSON.parse(data.toString()).traceEvents;
    assert(traces.length > 0);
  }));
}));
