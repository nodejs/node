import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';

const setupArgs = [
  '--no-warnings',
  '--input-type=module',
  '--eval',
];
const commonInput = `
import os from "node:os";
import url from "node:url";
`;
const commonArgs = [
  ...setupArgs,
  commonInput,
];

describe('ESM: loader concurrency', { concurrency: true }, () => {
  it('should load module requests concurrently', async () => {
    const { code, stderr, stdout } = await spawnPromisified(
      execPath,
      [
        '--loader',
        fixtures.fileURL('es-module-loaders', 'loader-resolve-passthru.mjs'),
        '--loader',
        fixtures.fileURL('es-module-loaders', 'loader-load-passthru.mjs'),
        ...commonArgs,
      ],
      { encoding: 'utf8' },
    );

    console.log(stdout);
    assert.strictEqual(stderr, '');
    // Verify that module requests are loaded concurrently without waiting for the previous one to finish.
    // For example, if the module requests are loaded serially, the following output would be printed:
    // ```
    // resolve passthru
    // load passthru
    // resolve passthru
    // load passthru
    // ```
    assert.match(stdout, new RegExp(`resolve passthru
resolve passthru
load passthru
load passthru`));
    assert.strictEqual(code, 0);
  });
});
