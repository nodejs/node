import { mustCall, spawnPromisified } from '../common/index.mjs';
import { ok, match, notStrictEqual } from 'node:assert';
import { spawn as spawnAsync } from 'node:child_process';
import { execPath } from 'node:process';
import { describe, it } from 'node:test';


describe('ESM: http import via CLI', { concurrency: true }, () => {
  const disallowedSpecifier = 'http://example.com';

  it('should throw disallowed error for insecure protocol', async () => {
    const { code, stderr } = await spawnPromisified(execPath, [
      '--experimental-network-imports',
      '--input-type=module',
      '--eval',
      `import ${JSON.stringify(disallowedSpecifier)}`,
    ]);

    notStrictEqual(code, 0);

    // [ERR_NETWORK_IMPORT_DISALLOWED]: import of 'http://example.com/' by
    //   …/[eval1] is not supported: http can only be used to load local
    // resources (use https instead).
    match(stderr, /ERR_NETWORK_IMPORT_DISALLOWED/);
    ok(stderr.includes(disallowedSpecifier));
  });

  it('should throw disallowed error for insecure protocol in REPL', () => {
    const child = spawnAsync(execPath, [
      '--experimental-network-imports',
      '--input-type=module',
    ]);
    child.stdin.end(`import ${JSON.stringify(disallowedSpecifier)}`);

    let stderr = '';
    child.stderr.setEncoding('utf8');
    child.stderr.on('data', (data) => stderr += data);
    child.on('close', mustCall((code, _signal) => {
      notStrictEqual(code, 0);

      // [ERR_NETWORK_IMPORT_DISALLOWED]: import of 'http://example.com/' by
      //   …/[stdin] is not supported: http can only be used to load local
      // resources (use https instead).
      match(stderr, /\[ERR_NETWORK_IMPORT_DISALLOWED\]/);
      ok(stderr.includes(disallowedSpecifier));
    }));
  });
});
