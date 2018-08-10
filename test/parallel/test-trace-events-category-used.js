'use strict';
const common = require('../common');
const assert = require('assert');
const cp = require('child_process');

if (!common.isMainThread)
  common.skip('process.chdir is not available in Workers');

const CODE = `
  const { internalBinding } = require('internal/test/binding');
  const { isTraceCategoryEnabled } = internalBinding('trace_events');
  console.log(
    isTraceCategoryEnabled("custom")
  );
`;

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();
process.chdir(tmpdir.path);

const procEnabled = cp.spawn(
  process.execPath,
  [ '--trace-event-categories', 'custom',
    // make test less noisy since internal/test/binding
    // emits a warning.
    '--no-warnings',
    '--expose-internals',
    '-e', CODE ]
);
let procEnabledOutput = '';

procEnabled.stdout.on('data', (data) => procEnabledOutput += data);
procEnabled.stderr.pipe(process.stderr);
procEnabled.once('exit', common.mustCall(() => {
  assert.strictEqual(procEnabledOutput, 'true\n');
}));

const procDisabled = cp.spawn(
  process.execPath,
  [ '--trace-event-categories', 'other',
    // make test less noisy since internal/test/binding
    // emits a warning.
    '--no-warnings',
    '--expose-internals',
    '-e', CODE ]
);
let procDisabledOutput = '';

procDisabled.stdout.on('data', (data) => procDisabledOutput += data);
procDisabled.stderr.pipe(process.stderr);
procDisabled.once('exit', common.mustCall(() => {
  assert.strictEqual(procDisabledOutput, 'false\n');
}));
