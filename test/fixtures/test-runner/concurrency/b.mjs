import tmpdir from '../../../common/tmpdir.js';
import { setTimeout } from 'node:timers/promises';
import fs from 'node:fs/promises';
import path from 'node:path';

while (true) {
  const file = await fs.readFile(tmpdir.resolve('test-runner-concurrency'), 'utf8');
  if (file === 'a.mjs') {
    await fs.writeFile(tmpdir.resolve('test-runner-concurrency'), 'b.mjs');
    break;
  }
  await setTimeout(10);
}
