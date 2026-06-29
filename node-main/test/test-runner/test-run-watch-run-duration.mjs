// Test run({ watch: true }) recalculates the run duration on a watch restart
// This test is flaky by its nature as it relies on the timing of 2 different runs
// considering the number of digits in the duration_ms is 9
// the chances of having the same duration_ms are very low
// but not impossible
// In case of costant failures, consider increasing the number of tests
import '../common/index.mjs';
import assert from 'node:assert';
import { skipIfNoWatch, refreshForTestRunnerWatch, testRunnerWatch } from '../common/watch.js';

skipIfNoWatch();
refreshForTestRunnerWatch();

const testRuns = await testRunnerWatch({ file: 'test.js', fileToUpdate: 'test.js', useRunApi: true });
const durations = testRuns.map((run) => {
  const runDuration = run.match(/# duration_ms\s([\d.]+)/);
  return runDuration;
});
assert.notDeepStrictEqual(durations[0][1], durations[1][1]);
