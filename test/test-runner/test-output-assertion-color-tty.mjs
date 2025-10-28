// Flags: --expose-internals
// Test that the output of test-runner/output/assertion-color-tty.mjs matches
// test-runner/output/assertion-color-tty.snapshot
import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, specTransform, canColorize, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

if (!canColorize()) {
  common.skip('TTY colors not supported');
}

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/assertion-color-tty.mjs'),
  specTransform,
  { flags: ['--test', '--stack-trace-limit=0'], tty: true },
);
