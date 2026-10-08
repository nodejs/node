// This tests that cpSync throws an error if attempt is made to copy socket.
import * as common from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync } from 'node:fs';
import { createServer } from 'node:net';
import tmpdir from '../common/tmpdir.js';

const isWindows = process.platform === 'win32';
if (isWindows) {
  common.skip('No socket support on Windows');
}

// See https://github.com/nodejs/node/pull/48409
if (common.isInsideDirWithUnusualChars) {
  common.skip('Test is broken in directories with unusual characters');
}

tmpdir.refresh();

{
  const dest = nextdir();
  const server = createServer();
  server.listen(common.PIPE);
  assert.throws(
    () => cpSync(common.PIPE, dest),
    { code: 'ERR_FS_CP_SOCKET' }
  );
  server.close();
}
