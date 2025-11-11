// Test run({ watch: true }) runs tests with dependency repeatedly
import '../common/index.mjs';
import { skipIfNoWatch, refreshForTestRunnerWatch, testRunnerWatch } from '../common/watch.js';

skipIfNoWatch();
refreshForTestRunnerWatch();

await testRunnerWatch({ file: 'test.js', fileToUpdate: 'dependency.js', useRunApi: true });
