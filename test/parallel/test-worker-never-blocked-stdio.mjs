import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { execPath } from 'node:process';
import { spawn } from 'node:child_process';
import { once } from 'node:events';

const cp = spawn(execPath, [fixtures.path('worker-stdio.mjs')]);
try {
  await Promise.all([
    once(cp.stdout, 'data').then((data) => {
      assert.match(data.toString(), /^foo\r?\n$/);
    }),
    once(cp.stderr, 'data').then((data) => {
      assert.match(data.toString(), /^bar\r?\n$/);
    }),
  ]);
} finally {
  cp.kill();
}
