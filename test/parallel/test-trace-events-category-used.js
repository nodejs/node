'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

const CODE = `console.log(
  process.binding("trace_events").categoryGroupEnabled("custom")
);`;

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
process.chdir(tmpdir.path);

const procEnabled = cp.spawn(
  process.execPath,
  [ '--trace-event-categories', 'custom', '-e', CODE ]
);
let procEnabledOutput = '';

procEnabled.stdout.on('data', (data) => procEnabledOutput += data);
procEnabled.stderr.pipe(process.stderr);
procEnabled.once('exit', common.mustCall(() => {
  assert.strictEqual(procEnabledOutput, 'true\n');
}));

const procDisabled = cp.spawn(
  process.execPath,
  [ '--trace-event-categories', 'other', '-e', CODE ]
);
let procDisabledOutput = '';

procDisabled.stdout.on('data', (data) => procDisabledOutput += data);
procDisabled.stderr.pipe(process.stderr);
procDisabled.once('exit', common.mustCall(() => {
  assert.strictEqual(procDisabledOutput, 'false\n');
}));
