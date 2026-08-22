'use strict';

// Verifies that a host binary with the SEA fuse set but without a
// NODE_SEA_BLOB resource exits with a clear error instead of SIGSEGV.
// Regression test for https://github.com/nodejs/node/issues/63466.

require('../common');

const {
  skipIfSingleExecutableIsNotSupported,
  signSEA,
} = require('../common/sea');

skipIfSingleExecutableIsNotSupported();

const tmpdir = require('../common/tmpdir');
const { copyFileSync, readFileSync, writeFileSync, chmodSync } = require('fs');
const { join } = require('path');
const { spawnSyncAndAssert } = require('../common/child_process');

tmpdir.refresh();

const fusedBinary = join(tmpdir.path, process.platform === 'win32' ? 'fused.exe' : 'fused');
copyFileSync(process.execPath, fusedBinary);

const fuse = Buffer.from('NODE_SEA_FUSE_fce680ab2cc467b6e072b8b5df1996b2');
const buf = readFileSync(fusedBinary);
const fuseAt = buf.indexOf(fuse);
if (fuseAt === -1) {
  require('../common').skip('SEA fuse sentinel not found in process.execPath');
}

const fuseValueOffset = fuseAt + fuse.length + 1; // skip ':'
if (buf[fuseValueOffset] !== 0x30 /* '0' */) {
  require('../common').skip(`Unexpected SEA fuse value: ${buf[fuseValueOffset]}`);
}

buf[fuseValueOffset] = 0x31; // '1'
writeFileSync(fusedBinary, buf);
chmodSync(fusedBinary, 0o755);
signSEA(fusedBinary);

spawnSyncAndAssert(
  fusedBinary,
  ['--version'],
  {},
  {
    status: 1,
    signal: null,
    stderr: /SEA fuse is set but no valid NODE_SEA_BLOB resource was found/,
  },
);
