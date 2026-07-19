// Test --test --watch --test-isolation=process runs tests without a file argument
import '../common/index.mjs';
import { skipIfNoWatch, refreshForTestRunnerWatch, testRunnerWatch } from '../common/watch.js';

skipIfNoWatch();
refreshForTestRunnerWatch();

await testRunnerWatch({ fileToUpdate: 'test.js', isolation: 'process' });
