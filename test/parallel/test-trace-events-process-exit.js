'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');

const FILE_NAME = 'node_trace.1.log';

common.refreshTmpDir();
process.chdir(common.tmpDir);

const proc = cp.spawn(process.execPath,
                      [ '--trace-events-enabled',
                        '-e', 'process.exit()' ]);

proc.once('exit', common.mustCall(() => {
  assert(common.fileExists(FILE_NAME));
  fs.readFile(FILE_NAME, common.mustCall((err, data) => {
    const traces = JSON.parse(data.toString()).traceEvents;
    assert(traces.length > 0);
  }));
}));
