'use strict';

// Security-hardening regression tests for node:zlib ZIP support. Each test
// describes a distinct issue found by audit and asserts the *secure* behavior,
// so every test fails on the pre-fix code and passes once its fix lands.
//
//  1. Local-vs-central header confusion (parser-confusion / inspect-then-consume
//     divergence): the central directory is authoritative in this reader, but a
//     local file header that disagrees on method, sizes, CRC, or the encryption
//     flag lets another ZIP tool (which extracts from the local header) read a
//     different member from the same archive. Such an archive must be rejected.
//  2. The archiver (zipFiles) must not block forever on a FIFO/special source.
//  3. The streaming read path (contentIterator) is hard-bounded by the header's
//     declared uncompressed size: a member that inflates past it is rejected
//     mid-stream, so entry.size is a ceiling a consumer can trust up front.

require('../common');
const assert = require('assert');
const { test } = require('node:test');
const zlib = require('zlib');
const path = require('path');
const { spawnSync } = require('child_process');
const tmpdir = require('../common/tmpdir');

const SIG_LOCAL = 0x04034b50;
const SIG_CENTRAL = 0x02014b50;

// A minimal single-member archive; caller patches its headers.
function buildStored(name, content, method = 'store') {
  const entry = zlib.ZipEntry.createSync(name, Buffer.from(content), { method });
  const chunks = [];
  for (const chunk of zlib.createZipArchiveSync([entry])) chunks.push(chunk);
  return Buffer.concat(chunks);
}

function centralOffset(buf) {
  for (let i = buf.length - 22; i >= 0; i--) {
    if (buf.readUInt32LE(i) === SIG_CENTRAL) return i;
  }
  throw new Error('no central directory header found');
}

// A ZipBuffer parses the central directory; reading a member resolves and
// checks its local header. Do both so the assertion holds whether the check
// is eager (parse time) or lazy (read time).
function readsThrow(buf, name) {
  assert.throws(() => {
    const zb = new zlib.ZipBuffer(buf);
    zb.get(name).contentSync();
  }, { code: 'ERR_ZIP_INVALID_ARCHIVE' });
}

// 1a. Local vs central compressed/uncompressed size disagreement.
test('a local/central size disagreement is rejected', () => {
  const buf = buildStored('a.txt', 'hello');
  assert.strictEqual(buf.readUInt32LE(0), SIG_LOCAL);
  buf.writeUInt32LE(999, 18); // Local compressed size
  buf.writeUInt32LE(999, 22); // Local uncompressed size
  readsThrow(buf, 'a.txt');
});

// 1b. Local vs central compression-method disagreement.
test('a local/central method disagreement is rejected', () => {
  const buf = buildStored('a.txt', 'hello');
  buf.writeUInt16LE(8, 8); // Local method deflate; central stays store(0)
  readsThrow(buf, 'a.txt');
});

// 1c. Central marks the member encrypted while the local header does not:
// a central-directory reader (e.g. python) treats it as opaque/encrypted, so
// Node must not silently decode it either.
test('a local/central encryption-flag disagreement is rejected', () => {
  const buf = buildStored('a.txt', 'hello');
  const c = centralOffset(buf);
  buf.writeUInt16LE(buf.readUInt16LE(c + 8) | 0x0001, c + 8); // Central encrypted bit
  readsThrow(buf, 'a.txt');
});

// 2. zipFiles must reject a FIFO source rather than block on open() forever.
test('zipFiles rejects a FIFO source instead of hanging', () => {
  if (process.platform === 'win32') return; // no mkfifo
  tmpdir.refresh();
  const fifo = path.join(tmpdir.path, 'evil.fifo');
  if (spawnSync('mkfifo', [fifo]).status !== 0) return; // mkfifo unavailable
  const script =
    'const zlib = require("zlib");' +
    '(async () => {' +
    '  try {' +
    `    for await (const _ of zlib.zipFiles([[${JSON.stringify(fifo)}, "x"]], ` +
    '      { followSymlinks: false })) {}' +
    '    console.log("COMPLETED");' +
    '  } catch (e) { console.log("REJECTED:" + e.code); }' +
    '})();';
  const res = spawnSync(process.execPath, ['--no-warnings', '-e', script],
                        { timeout: 5000, encoding: 'utf8' });
  assert.ok(res.signal === null,
            'zipFiles hung on a FIFO source (killed by timeout)');
  assert.match(res.stdout, /REJECTED:ERR_ZIP_UNSUPPORTED_FEATURE/);
});

// 3. Streaming is hard-bounded by the declared uncompressed size: a member
// whose data inflates past it is rejected mid-stream, so a consumer can trust
// entry.size as the ceiling before choosing to buffer.
test('contentIterator rejects a member that inflates past its declared size', async () => {
  const big = zlib.ZipEntry.createSync(
    'big', Buffer.alloc(64 * 1024), { method: 'deflate' });
  const chunks = [];
  for (const chunk of zlib.createZipArchiveSync([big])) chunks.push(chunk);
  const buf = Buffer.concat(chunks);
  // Shrink the declared uncompressed size in both headers (kept consistent so
  // the header cross-check passes) below what the data actually inflates to.
  const c = centralOffset(buf);
  buf.writeUInt32LE(100, 22); // Local uncompressed size
  buf.writeUInt32LE(100, c + 24); // Central uncompressed size
  const entry = new zlib.ZipBuffer(buf).get('big');
  await assert.rejects(async () => {
    // eslint-disable-next-line no-unused-vars
    for await (const _ of entry.contentIterator()) { /* drain */ }
  }, { code: 'ERR_ZIP_ENTRY_CORRUPT', message: /inflates beyond its declared size/ });
});
