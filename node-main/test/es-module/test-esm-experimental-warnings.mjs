import { spawnPromisified } from '../common/index.mjs';
import { fileURL } from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';


describe('ESM: warn for obsolete hooks provided', { concurrency: !process.env.TEST_PARALLEL }, () => {
  it('should not print warnings when no experimental features are enabled or used', async () => {
    const { code, signal, stderr } = await spawnPromisified(execPath, [
      '--input-type=module',
      '--eval',
      `import ${JSON.stringify(fileURL('es-module-loaders', 'module-named-exports.mjs'))}`,
    ]);

    assert.doesNotMatch(
      stderr,
      /ExperimentalWarning/,
      new Error('No experimental warning(s) should be emitted when no experimental feature is enabled')
    );
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });

  describe('experimental warnings for enabled experimental feature', () => {
    for (
      const [experiment, ...args] of [
        [
          /`--experimental-loader` may be removed in the future/,
          '--experimental-loader',
          fileURL('es-module-loaders', 'hooks-custom.mjs'),
        ],
      ]
    ) {
      it(`should print for ${experiment.toString().replaceAll('/', '')}`, async () => {
        const { code, signal, stderr } = await spawnPromisified(execPath, [
          ...args,
          '--input-type=module',
          '--eval',
          `import ${JSON.stringify(fileURL('es-module-loaders', 'module-named-exports.mjs'))}`,
        ]);

        assert.match(stderr, /ExperimentalWarning/);
        assert.match(stderr, experiment);
        assert.strictEqual(code, 0);
        assert.strictEqual(signal, null);
      });
    }
  });
});
