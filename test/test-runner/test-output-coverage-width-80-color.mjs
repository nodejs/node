// Flags: --expose-internals
// Test that the output of test-runner/output/coverage-width-80-color.mjs matches
// test-runner/output/coverage-width-80-color.snapshot
import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, specTransform, canColorize, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

if (!process.features.inspector) {
  common.skip('inspector support required');
}

if (!canColorize()) {
  common.skip('TTY colors not supported');
}

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/coverage-width-80-color.mjs'),
  specTransform,
  { flags: ['--test-coverage-exclude=!test/**'], tty: true },
);
