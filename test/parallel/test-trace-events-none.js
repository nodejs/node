'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');

if (!common.isMainThread)
  common.skip('process.chdir is not available in Workers');

const CODE =
  'setTimeout(() => { for (var i = 0; i < 100000; i++) { "test" + i } }, 1)';
const FILE_NAME = 'node_trace.1.log';

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
process.chdir(tmpdir.path);

const proc_no_categories = cp.spawn(
  process.execPath,
  [ '--trace-event-categories', '""', '-e', CODE ]
);

proc_no_categories.once('exit', common.mustCall(() => {
  assert(common.fileExists(FILE_NAME));
  // Only __metadata categories should have been emitted.
  fs.readFile(FILE_NAME, common.mustCall((err, data) => {
    assert.ok(JSON.parse(data.toString()).traceEvents.every(
      (trace) => trace.cat === '__metadata'));
  }));
}));
