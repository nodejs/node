// This tests that cpSync throws an error if attempt is made to copy socket.
import * as common from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync, mkdirSync } from 'node:fs';
import { createServer } from 'node:net';
import { join } from 'node:path';
import tmpdir from '../common/tmpdir.js';

const isWindows = process.platform === 'win32';
if (isWindows) {
  common.skip('No socket support on Windows');
}

// See https://github.com/nodejs/node/pull/48409
if (common.isInsideDirWithUnusualChars) {
  common.skip('Test is borken in directories with unusual characters');
}

tmpdir.refresh();

{
  const src = nextdir();
  mkdirSync(src);
  const dest = nextdir();
  const sock = join(src, `${process.pid}.sock`);
  const server = createServer();
  server.listen(sock);
  assert.throws(
    () => cpSync(sock, dest),
    { code: 'ERR_FS_CP_SOCKET' }
  );
  server.close();
}
