// Flags: --experimental-vfs --expose-internals
'use strict';

// Exercise the default-option paths in createFileStats / createDirectoryStats
// / createSymlinkStats / createZeroStats. These defaults aren't taken when
// MemoryProvider populates every option from the entry, so we drive the
// helpers directly.

require('../common');
const assert = require('assert');
const {
  createFileStats,
  createDirectoryStats,
  createSymlinkStats,
  createZeroStats,
} = require('internal/vfs/stats');

// All defaults — no options object at all
{
  const st = createFileStats(42);
  assert.strictEqual(st.size, 42);
  assert.strictEqual((st.mode & 0o777), 0o644);
  assert.strictEqual(st.nlink, 1);
  assert.ok(st.isFile());

  const dirSt = createDirectoryStats();
  assert.ok(dirSt.isDirectory());
  assert.strictEqual((dirSt.mode & 0o777), 0o755);

  const linkSt = createSymlinkStats(7);
  assert.ok(linkSt.isSymbolicLink());
  assert.strictEqual((linkSt.mode & 0o777), 0o777);
  assert.strictEqual(linkSt.size, 7);
}

// Empty options object exercises the `?? default` right-hand side.
{
  const st = createFileStats(1, {});
  assert.ok(st.isFile());
  const dirSt = createDirectoryStats({});
  assert.ok(dirSt.isDirectory());
  const linkSt = createSymlinkStats(0, {});
  assert.ok(linkSt.isSymbolicLink());
}

// Bigint variant of zero-stats
{
  const z = createZeroStats({ bigint: true });
  assert.strictEqual(typeof z.size, 'bigint');
  assert.strictEqual(z.size, 0n);
  assert.strictEqual(z.mode, 0n);
}

// Non-bigint zero-stats with no options
{
  const z = createZeroStats();
  assert.strictEqual(z.size, 0);
  assert.strictEqual(z.mode, 0);
}

// Cover the `process.getuid?.() ?? 0` fallback (Windows-like environment).
// We stub the optional methods to simulate their absence.
{
  const realUid = process.getuid;
  const realGid = process.getgid;
  process.getuid = undefined;
  process.getgid = undefined;
  try {
    const fs = createFileStats(0);
    assert.strictEqual(fs.uid, 0);
    assert.strictEqual(fs.gid, 0);
    const ds = createDirectoryStats();
    assert.strictEqual(ds.uid, 0);
    const ls = createSymlinkStats(0);
    assert.strictEqual(ls.uid, 0);
  } finally {
    process.getuid = realUid;
    process.getgid = realGid;
  }
}
