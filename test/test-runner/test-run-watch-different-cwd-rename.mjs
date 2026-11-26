// Test run({ watch: true }) handles renames in a different cwd
import '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';
import { skipIfNoWatch, refreshForTestRunnerWatch, testRunnerWatch } from '../common/watch.js';

skipIfNoWatch();
refreshForTestRunnerWatch();

await testRunnerWatch({
  file: tmpdir.resolve('test.js'),
  fileToUpdate: 'test.js',
  cwd: import.meta.dirname,
  action: 'rename2',
  useRunApi: true,
});
