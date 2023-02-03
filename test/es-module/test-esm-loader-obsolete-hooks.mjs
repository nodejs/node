import { spawnPromisified } from '../common/index.mjs';
import { fileURL, path } from '../common/fixtures.mjs';
import { match, notStrictEqual } from 'node:assert';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';


describe('ESM: deprecation warnings for obsolete hooks', { concurrency: true }, () => {
  it(async () => {
    const { code, stderr } = await spawnPromisified(execPath, [
      '--no-warnings',
      '--throw-deprecation',
      '--experimental-loader',
      fileURL('es-module-loaders', 'hooks-obsolete.mjs').href,
      path('print-error-message.js'),
    ]);

    // DeprecationWarning: Obsolete loader hook(s) supplied and will be ignored:
    // dynamicInstantiate, getFormat, getSource, transformSource
    match(stderr, /DeprecationWarning:/);
    match(stderr, /dynamicInstantiate/);
    match(stderr, /getFormat/);
    match(stderr, /getSource/);
    match(stderr, /transformSource/);

    notStrictEqual(code, 0);
  });
});
