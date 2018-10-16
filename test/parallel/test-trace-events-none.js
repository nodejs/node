'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');
const path = require('path');

const CODE =
  'setTimeout(() => { for (var i = 0; i < 100000; i++) { "test" + i } }, 1)';

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
const FILE_NAME = path.join(tmpdir.path, 'node_trace.1.log');

const proc_no_categories = cp.spawn(
  process.execPath,
  [ '--trace-event-categories', '""', '-e', CODE ],
  { cwd: tmpdir.path }
);

proc_no_categories.once('exit', common.mustCall(() => {
  assert(fs.existsSync(FILE_NAME));
  // Only __metadata categories should have been emitted.
  fs.readFile(FILE_NAME, common.mustCall((err, data) => {
    assert.ok(JSON.parse(data.toString()).traceEvents.every(
      (trace) => trace.cat === '__metadata'));
  }));
}));
