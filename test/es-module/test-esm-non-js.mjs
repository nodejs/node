import { spawnPromisified } from '../common/index.mjs';
import { fileHref } from '../common/fixtures.mjs';
import { match, strictEqual } from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';


describe('ESM: non-js extensions fail', { concurrency: true }, () => {
  it(async () => {
    const { code, stderr, signal } = await spawnPromisified(execPath, [
      '--input-type=module',
      '--eval',
      `import '${fileHref('es-modules', 'file.unknown')}'`,
    ]);

    match(stderr, /ERR_UNKNOWN_FILE_EXTENSION/);
    strictEqual(code, 1);
    strictEqual(signal, null);
  });
});
