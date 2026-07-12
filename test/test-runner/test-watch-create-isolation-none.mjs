// Test --test --watch --test-isolation=none runs tests when a new file is created in the watched directory
import '../common/index.mjs';
import { skipIfNoWatch, refreshForTestRunnerWatch, testRunnerWatch } from '../common/watch.js';

skipIfNoWatch();
refreshForTestRunnerWatch();

await testRunnerWatch({ action: 'create', fileToCreate: 'new-test-file.test.js', isolation: 'none' });
