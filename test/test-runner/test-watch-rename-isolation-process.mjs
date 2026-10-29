// Test --test --watch --test-isolation=process handles a watched test file rename
import '../common/index.mjs';
import { skipIfNoWatch, refreshForTestRunnerWatch, testRunnerWatch } from '../common/watch.js';

skipIfNoWatch();
refreshForTestRunnerWatch();

await testRunnerWatch({ fileToUpdate: 'test.js', action: 'rename', isolation: 'process' });
