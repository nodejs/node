import '../common/index.mjs';
import assert from 'assert';

await assert.rejects(
  import('../fixtures/file-to-read-without-bom.txt', { with: { type: 'text' } }),
  { code: 'ERR_UNKNOWN_FILE_EXTENSION' },
);

await assert.rejects(
  import('data:text/plain,hello%20world', { with: { type: 'text' } }),
  { code: 'ERR_UNKNOWN_MODULE_FORMAT' },
);

await assert.rejects(
  import('../fixtures/file-to-read-without-bom.txt', { with: { type: 'bytes' } }),
  { code: 'ERR_UNKNOWN_FILE_EXTENSION' },
);

await assert.rejects(
  import('data:text/plain,hello%20world', { with: { type: 'bytes' } }),
  { code: 'ERR_UNKNOWN_MODULE_FORMAT' },
);
