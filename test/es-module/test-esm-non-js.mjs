import '../common/index.mjs';
import { fileURL } from '../common/fixtures.mjs';
import { match, strictEqual } from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';

import spawn from './helper.spawnAsPromised.mjs';

describe('ESM: non-js extensions fail', { concurrency: true }, () => {
  it(async () => {
    const { code, stderr, signal } = await spawn(execPath, [
      '--input-type=module',
      '--eval',
      `import ${JSON.stringify(fileURL('es-modules', 'file.unknown'))}`,
    ]);

    match(stderr, /ERR_UNKNOWN_FILE_EXTENSION/);
    strictEqual(code, 1);
    strictEqual(signal, null);
  });
});
