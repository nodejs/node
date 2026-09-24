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

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
const FILE_NAME = tmpdir.resolve(defaultTraceFileName);

const proc = cp.spawn(process.execPath,
                      [ '--trace-events-enabled',
                        '-e', 'process.exit()' ],
                      { cwd: tmpdir.path });

proc.once('exit', common.mustCall(() => {
  assert(fs.existsSync(FILE_NAME));
  assert(readTraceEvents(FILE_NAME).length > 0);
}));
