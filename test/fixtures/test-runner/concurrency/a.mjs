import tmpdir from '../../../common/tmpdir.js';
import { setTimeout } from 'node:timers/promises';
import fs from 'node:fs/promises';
import path from 'node:path';

await fs.writeFile(path.resolve(tmpdir.path, 'test-runner-concurrency'), 'a.mjs');
while (true) {
  const file = await fs.readFile(path.resolve(tmpdir.path, 'test-runner-concurrency'), 'utf8');
  if (file === 'b.mjs') {
    break;
  }
  await setTimeout(10);
}
