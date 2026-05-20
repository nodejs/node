// Test run({ watch: true }) runs tests with dependency repeatedly in a different cwd
import '../common/index.mjs';
import { join } from 'node:path';
import tmpdir from '../common/tmpdir.js';
import { skipIfNoWatch, refreshForTestRunnerWatch, testRunnerWatch } from '../common/watch.js';

skipIfNoWatch();
refreshForTestRunnerWatch();

await testRunnerWatch({
  file: join(tmpdir.path, 'test.js'),
  fileToUpdate: 'dependency.js',
  cwd: import.meta.dirname,
  action: 'rename2',
  useRunApi: true,
});
