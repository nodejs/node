import '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';

import assert from 'node:assert';
import { open, writeFile } from 'node:fs/promises';

tmpdir.refresh();

const filePath = tmpdir.resolve('file.txt');

await writeFile(filePath, '1\n\n2\n');

let file;
try {
  file = await open(filePath);

  let i = 0;
  for await (const line of file.readLines()) {
    switch (i++) {
      case 0:
        assert.strictEqual(line, '1');
        break;

      case 1:
        assert.strictEqual(line, '');
        break;

      case 2:
        assert.strictEqual(line, '2');
        break;

      default:
        assert.fail();
        break;
    }
  }
} finally {
  await file?.close();
}
