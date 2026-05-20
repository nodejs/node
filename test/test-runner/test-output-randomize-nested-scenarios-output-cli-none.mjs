// Test that the output of test-runner/output/randomize_nested_scenarios_output_cli_none.js
// matches test-runner/output/randomize_nested_scenarios_output_cli_none.snapshot
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawnAndAssert, specTransform, ensureCwdIsProjectRoot } from '../common/assertSnapshot.js';

ensureCwdIsProjectRoot();
await spawnAndAssert(
  fixtures.path('test-runner/output/randomize_nested_scenarios_output_cli_none.js'),
  specTransform,
);
