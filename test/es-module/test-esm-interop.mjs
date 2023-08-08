import '../common/index.mjs';
import assert from 'node:assert';
import { __filename, __dirname } from 'node:interop';
import { dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const filename = fileURLToPath(import.meta.url);

assert.strictEqual(__dirname, dirname(filename));
assert.strictEqual(__filename, (filename));

await assert.rejects(
  import('data:text/javascript,import"node:interop"'),
  { code: 'ERR_INVALID_URL_SCHEME' },
);
