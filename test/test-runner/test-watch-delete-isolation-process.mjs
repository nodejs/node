// Test --test --watch --test-isolation=process does not throw when deleting a watched test file
import '../common/index.mjs';
import { skipIfNoWatch, refreshForTestRunnerWatch, testRunnerWatch } from '../common/watch.js';

skipIfNoWatch();
refreshForTestRunnerWatch();

await testRunnerWatch({ fileToUpdate: 'test.js', action: 'delete', isolation: 'process' });
