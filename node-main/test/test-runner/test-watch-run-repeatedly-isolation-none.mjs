// Test --test --watch --test-isolation=none runs tests repeatedly
import '../common/index.mjs';
import { skipIfNoWatch, refreshForTestRunnerWatch, testRunnerWatch } from '../common/watch.js';

skipIfNoWatch();
refreshForTestRunnerWatch();

await testRunnerWatch({ file: 'test.js', fileToUpdate: 'test.js', isolation: 'none' });
