// Flags: --experimental-vfs --expose-internals
'use strict';

// Cover the VirtualFileHandle base class — all primitives must throw
// ERR_METHOD_NOT_IMPLEMENTED until a subclass overrides them.

const common = require('../common');
const assert = require('assert');
const { VirtualFileHandle } = require('internal/vfs/file_handle');

const handle = new VirtualFileHandle('/x.txt', 'r', 0o600);
assert.strictEqual(handle.path, '/x.txt');
assert.strictEqual(handle.flags, 'r');
assert.strictEqual(handle.mode, 0o600);
assert.strictEqual(handle.position, 0);
assert.strictEqual(handle.closed, false);

// Sync stubs throw ERR_METHOD_NOT_IMPLEMENTED
for (const m of ['readSync', 'writeSync', 'readFileSync', 'writeFileSync',
                 'statSync', 'truncateSync']) {
  assert.throws(() => handle[m](), { code: 'ERR_METHOD_NOT_IMPLEMENTED' },
                `${m} should throw`);
}

// Async stubs reject
for (const m of ['read', 'write', 'readFile', 'writeFile', 'stat', 'truncate']) {
  assert.rejects(handle[m](), { code: 'ERR_METHOD_NOT_IMPLEMENTED' },
                 `${m} should reject`).then(common.mustCall());
}

// readv/writev base stubs throw
assert.rejects(handle.readv([Buffer.alloc(1)], 0),
               { code: 'ERR_METHOD_NOT_IMPLEMENTED' }).then(common.mustCall());
assert.rejects(handle.writev([Buffer.alloc(1)], 0),
               { code: 'ERR_METHOD_NOT_IMPLEMENTED' }).then(common.mustCall());

// appendFile uses write under the hood — same not-implemented
assert.rejects(handle.appendFile('x'),
               { code: 'ERR_METHOD_NOT_IMPLEMENTED' }).then(common.mustCall());

// readableWebStream / readLines / createReadStream / createWriteStream throw
assert.throws(() => handle.readableWebStream(),
              { code: 'ERR_METHOD_NOT_IMPLEMENTED' });
assert.throws(() => handle.readLines(),
              { code: 'ERR_METHOD_NOT_IMPLEMENTED' });
assert.throws(() => handle.createReadStream(),
              { code: 'ERR_METHOD_NOT_IMPLEMENTED' });
assert.throws(() => handle.createWriteStream(),
              { code: 'ERR_METHOD_NOT_IMPLEMENTED' });

// No-op metadata
(async () => {
  await handle.chmod();
  await handle.chown();
  await handle.utimes();
  await handle.datasync();
  await handle.sync();
})().then(common.mustCall());

// close() / closeSync() / dispose
{
  const h = new VirtualFileHandle('/y', 'r');
  h.closeSync();
  assert.strictEqual(h.closed, true);

  // Operations after close throw EBADF (via #checkClosed) before NOT_IMPL
  assert.throws(() => h.readSync(), { code: 'EBADF' });
  assert.rejects(h.read(), { code: 'EBADF' }).then(common.mustCall());
}

// Close via async + Symbol.asyncDispose
(async () => {
  const h = new VirtualFileHandle('/z', 'r');
  await h.close();
  assert.strictEqual(h.closed, true);

  const h2 = new VirtualFileHandle('/z2', 'r');
  await h2[Symbol.asyncDispose]();
  assert.strictEqual(h2.closed, true);
})().then(common.mustCall());

// truncateSync default len = 0 path requires close-check too
{
  const h = new VirtualFileHandle('/a', 'r');
  h.closeSync();
  assert.throws(() => h.truncateSync(), { code: 'EBADF' });
  assert.rejects(h.truncate(), { code: 'EBADF' }).then(common.mustCall());
}
