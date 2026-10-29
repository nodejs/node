// Test --test --watch --test-isolation=none runs tests with ESM dependency
import '../common/index.mjs';
import { skipIfNoWatch, refreshForTestRunnerWatch, testRunnerWatch } from '../common/watch.js';

skipIfNoWatch();
refreshForTestRunnerWatch();

await testRunnerWatch({ file: 'test.js', fileToUpdate: 'dependency.mjs', isolation: 'none' });
