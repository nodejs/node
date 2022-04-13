'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

const CODE = `
  const { internalBinding } = require('internal/test/binding');
  const { isTraceCategoryEnabled } = internalBinding('trace_events');
  console.log(
    isTraceCategoryEnabled("custom")
  );
`;

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const procEnabled = cp.spawn(
  process.execPath,
  [ '--trace-event-categories', 'custom',
    // Make test less noisy since internal/test/binding
    // emits a warning.
    '--no-warnings',
    '--expose-internals',
    '-e', CODE ],
  { cwd: tmpdir.path }
);
let procEnabledOutput = '';

procEnabled.stdout.on('data', (data) => procEnabledOutput += data);
procEnabled.stderr.pipe(process.stderr);
procEnabled.once('close', common.mustCall(() => {
  assert.strictEqual(procEnabledOutput, 'true\n');
}));

const procDisabled = cp.spawn(
  process.execPath,
  [ '--trace-event-categories', 'other',
    // Make test less noisy since internal/test/binding
    // emits a warning.
    '--no-warnings',
    '--expose-internals',
    '-e', CODE ],
  { cwd: tmpdir.path }
);
let procDisabledOutput = '';

procDisabled.stdout.on('data', (data) => procDisabledOutput += data);
procDisabled.stderr.pipe(process.stderr);
procDisabled.once('close', common.mustCall(() => {
  assert.strictEqual(procDisabledOutput, 'false\n');
}));
