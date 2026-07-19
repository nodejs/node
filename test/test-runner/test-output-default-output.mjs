// Test that the output of test-runner/output/default_output.js matches test-runner/output/default_output.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, specTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/default_output.js'),
  specTransform,
  { tty: true },
);
