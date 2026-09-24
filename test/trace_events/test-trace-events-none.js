'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');

const {
  defaultTraceFileName,
  readTraceEvents,
  checkTraceProcessor,
} = require('../common/trace_events');

checkTraceProcessor();

const CODE =
  'setTimeout(() => { for (let i = 0; i < 100000; i++) { "test" + i } }, 1)';

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
const FILE_NAME = tmpdir.resolve(defaultTraceFileName);

const proc_no_categories = cp.spawn(
  process.execPath,
  [ '--trace-event-categories', '""', '-e', CODE ],
  { cwd: tmpdir.path },
);

proc_no_categories.once('exit', common.mustCall(() => {
  assert(fs.existsSync(FILE_NAME));
  // Only __metadata categories should have been emitted.
  assert.ok(readTraceEvents(FILE_NAME).every(
    (trace) => trace.cat === '__metadata'));
}));
