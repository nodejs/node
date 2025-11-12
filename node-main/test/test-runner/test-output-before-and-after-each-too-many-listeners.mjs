// Test that the output of test-runner/output/before-and-after-each-too-many-listeners.js matches
// test-runner/output/before-and-after-each-too-many-listeners.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, defaultTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/before-and-after-each-too-many-listeners.js'),
  defaultTransform,
  { flags: ['--test-reporter=tap'] },
);
