// This tests that cp() rejects a socket or a FIFO found inside the copied
// tree with the same errors as for a top-level one, while cpSync() skips them.

import * as common from '../common/index.mjs';
import assert from 'node:assert';
import { spawnSync } from 'node:child_process';
import { cpSync, existsSync, mkdirSync, writeFileSync, promises } from 'node:fs';
import { createServer } from 'node:net';
import { join } from 'node:path';
import { nextdir } from '../common/fs.js';
import tmpdir from '../common/tmpdir.js';

if (common.isWindows)
  common.skip('No socket/FIFO support on Windows');
if (common.isInsideDirWithUnusualChars)
  common.skip('Test is broken in directories with unusual characters');

tmpdir.refresh();

{
  const src = nextdir();
  mkdirSync(join(src, 'd'), { recursive: true });
  writeFileSync(join(src, 'd', 'file'), 'x');
  const server = createServer();
  // The socket path can exceed the platform limit in a deep tmpdir; skip then.
  const listening = await new Promise((resolve) => {
    server.on('error', () => resolve(false));
    server.listen(join(src, 'd', 's.sock'), () => resolve(true));
  });
  if (!listening) {
    common.printSkipMessage('socket path too long');
  } else {
    await assert.rejects(promises.cp(src, nextdir(), { recursive: true }), { code: 'ERR_FS_CP_SOCKET' });
    const dest = nextdir();
    cpSync(src, dest, { recursive: true });
    assert.ok(existsSync(join(dest, 'd', 'file')));
    server.close();
  }
}

{
  const src = nextdir();
  mkdirSync(join(src, 'dir'), { recursive: true });
  writeFileSync(join(src, 'dir', 'file'), 'x');
  if (spawnSync('mkfifo', [join(src, 'dir', 'fifo')]).status !== 0) {
    common.printSkipMessage('mkfifo not available');
  } else {
    await assert.rejects(promises.cp(src, nextdir(), { recursive: true }), { code: 'ERR_FS_CP_FIFO_PIPE' });
    const dest = nextdir();
    cpSync(src, dest, { recursive: true });
    assert.ok(existsSync(join(dest, 'dir', 'file')));
  }
}
