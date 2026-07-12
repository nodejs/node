// Test that the output of test-runner/output/randomize_output_cli.js matches
// test-runner/output/randomize_output_cli.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, specTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/randomize_output_cli.js'),
  specTransform,
);
