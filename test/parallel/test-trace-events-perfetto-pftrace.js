'use strict';
const common = require('../common');

if (!process.config.variables.v8_use_perfetto)
  common.skip('perfetto tracing is not enabled');

const assert = require('assert');
const cp = require('child_process');
const fs = require('fs');

const CODE =
  'setTimeout(() => { for (let i = 0; i < 100000; i++) { "test" + i } }, 1)';

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
// Perfetto builds default the file pattern to a .pftrace extension.
const FILE_NAME = tmpdir.resolve('node_trace.1.pftrace');

const proc = cp.spawn(process.execPath,
                      [ '--trace-events-enabled', '-e', CODE ],
                      { cwd: tmpdir.path });

proc.once('exit', common.mustCall(() => {
  assert(fs.existsSync(FILE_NAME));
  // The perfetto trace is a binary protobuf, so just check it has content.
  const stat = fs.statSync(FILE_NAME);
  assert(stat.size > 0);
}));
