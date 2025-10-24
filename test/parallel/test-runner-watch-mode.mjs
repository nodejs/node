import '../common/index.mjs';
import { skipIfNoWatch, refreshForTestRunnerWatch, testRunnerWatch } from '../common/watch.js';
import { describe, it, beforeEach } from 'node:test';

skipIfNoWatch();

describe('test runner watch mode', () => {
  beforeEach(refreshForTestRunnerWatch);
  for (const isolation of ['none', 'process']) {
    describe(`isolation: ${isolation}`, () => {
      it('should run tests repeatedly', async () => {
        await testRunnerWatch({ file: 'test.js', fileToUpdate: 'test.js', isolation });
      });

      it('should run tests with dependency repeatedly', async () => {
        await testRunnerWatch({ file: 'test.js', fileToUpdate: 'dependency.js', isolation });
      });

      it('should run tests with ESM dependency', async () => {
        await testRunnerWatch({ file: 'test.js', fileToUpdate: 'dependency.mjs', isolation });
      });

      it('should support running tests without a file', async () => {
        await testRunnerWatch({ fileToUpdate: 'test.js', isolation });
      });

      it('should support a watched test file rename', async () => {
        await testRunnerWatch({ fileToUpdate: 'test.js', action: 'rename', isolation });
      });

      it('should not throw when delete a watched test file', async () => {
        await testRunnerWatch({ fileToUpdate: 'test.js', action: 'delete', isolation });
      });

      it('should run new tests when a new file is created in the watched directory', {
        todo: isolation === 'none' ?
          'This test is failing when isolation is set to none and must be fixed' :
          undefined,
      }, async () => {
        await testRunnerWatch({ action: 'create', fileToCreate: 'new-test-file.test.js', isolation });
      });
    });
  }
});
