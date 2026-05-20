// Test run({ watch: true, cwd, isolation: 'process' }) executes using a
// different cwd for the runner than the process cwd with isolation process
import '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';
import { skipIfNoWatch, refreshForTestRunnerWatch, testRunnerWatch } from '../common/watch.js';

skipIfNoWatch();
refreshForTestRunnerWatch();

await testRunnerWatch({
  fileToUpdate: 'test.js',
  action: 'rename',
  cwd: import.meta.dirname,
  runnerCwd: tmpdir.path,
  isolation: 'process',
  useRunApi: true,
});
