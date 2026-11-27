// Flags: --disable-warning=ExperimentalWarning --expose-internals
'use strict';

// Exercise the default-option paths in createFileStats / createDirectoryStats
// / createSymlinkStats / createZeroStats. These defaults aren't taken when
// MemoryProvider populates every option from the entry, so we drive the
// helpers directly.

const common = require('../common');
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

// Timestamps before the epoch: whole-second and whole-millisecond values must
// round-trip unchanged (the sub-second part must not be applied twice).
{
  const st = createFileStats(0, {
    atimeMs: -500,
    mtimeMs: -1500,
    ctimeMs: -1000,
    birthtimeMs: -1,
  });
  assert.strictEqual(st.atimeMs, -500);
  assert.strictEqual(st.mtimeMs, -1500);
  assert.strictEqual(st.ctimeMs, -1000);
  assert.strictEqual(st.birthtimeMs, -1);
  assert.strictEqual(st.atime.getTime(), -500);
  assert.strictEqual(st.mtime.getTime(), -1500);
  assert.strictEqual(st.ctime.getTime(), -1000);
  assert.strictEqual(st.birthtime.getTime(), -1);

  const bigSt = createFileStats(0, {
    atimeMs: -500,
    mtimeMs: -1500,
    ctimeMs: -1000,
    birthtimeMs: -1,
    bigint: true,
  });
  assert.strictEqual(bigSt.atimeMs, -500n);
  assert.strictEqual(bigSt.mtimeMs, -1500n);
  assert.strictEqual(bigSt.ctimeMs, -1000n);
  assert.strictEqual(bigSt.birthtimeMs, -1n);
  assert.strictEqual(bigSt.atimeNs, -500_000_000n);
  assert.strictEqual(bigSt.mtimeNs, -1_500_000_000n);
  assert.strictEqual(bigSt.ctimeNs, -1_000_000_000n);
  assert.strictEqual(bigSt.birthtimeNs, -1_000_000n);
}

// Fractional and floating-point millisecond timestamps, including values whose
// conversion to nanoseconds is inexact, rounds up to a whole second, or would
// lose precision if 1000 were added to the remainder before scaling to ns.
{
  const fracBigSt = createFileStats(0, {
    atimeMs: -463.2,
    mtimeMs: -1500.1,
    ctimeMs: (1001 / 1000) * 1000,  // 1000.9999999999999
    birthtimeMs: 999.9999999,
    bigint: true,
  });
  assert.strictEqual(fracBigSt.atimeNs, -463_200_000n);
  assert.strictEqual(fracBigSt.mtimeNs, -1_500_100_000n);
  assert.strictEqual(fracBigSt.ctimeNs, 1_001_000_000n);
  assert.strictEqual(fracBigSt.ctimeMs, 1001n);
  assert.strictEqual(fracBigSt.birthtimeNs, 1_000_000_000n);
  assert.strictEqual(fracBigSt.birthtimeMs, 1000n);

  const fracSt = createFileStats(0, {
    atimeMs: -463.5,
    mtimeMs: -999.9999999,
    ctimeMs: (1001 / 1000) * 1000,
    birthtimeMs: 999.9999999,
  });
  assert.strictEqual(fracSt.atimeMs, -463.5);
  assert.strictEqual(fracSt.mtimeMs, -1000);
  assert.strictEqual(fracSt.ctimeMs, 1001);
  assert.strictEqual(fracSt.birthtimeMs, 1000);
}

// Sub-millisecond timestamps before the epoch.
{
  const subMsBigSt = createFileStats(0, {
    atimeMs: -5.000001e-7,
    mtimeMs: -0.0005075,
    ctimeMs: -0.0010005,
    birthtimeMs: -999.9999999,
    bigint: true,
  });
  assert.strictEqual(subMsBigSt.atimeNs, -1n);
  assert.strictEqual(subMsBigSt.mtimeNs, -508n);
  assert.strictEqual(subMsBigSt.ctimeNs, -1001n);
  assert.strictEqual(subMsBigSt.birthtimeNs, -1_000_000_000n);
  assert.strictEqual(subMsBigSt.birthtimeMs, -1000n);
}

// Values that are too small to be represented in whole nanoseconds must
// collapse to a positive zero rather than -0 or a one second shift.
{
  const underflowSt = createFileStats(0, {
    atimeMs: -Number.MIN_VALUE,
    mtimeMs: -1e-322,
    ctimeMs: -0.0000001,
    birthtimeMs: -0,
  });
  assert.strictEqual(underflowSt.atimeMs, 0);
  assert.strictEqual(underflowSt.mtimeMs, 0);
  assert.strictEqual(underflowSt.ctimeMs, 0);
  assert.strictEqual(underflowSt.birthtimeMs, 0);

  const underflowBigSt = createFileStats(0, {
    atimeMs: -Number.MIN_VALUE,
    mtimeMs: -1e-322,
    ctimeMs: -0.0000001,
    birthtimeMs: -0,
    bigint: true,
  });
  assert.strictEqual(underflowBigSt.atimeMs, 0n);
  assert.strictEqual(underflowBigSt.mtimeMs, 0n);
  assert.strictEqual(underflowBigSt.ctimeMs, 0n);
  assert.strictEqual(underflowBigSt.birthtimeMs, 0n);
  assert.strictEqual(underflowBigSt.atimeNs, 0n);
  assert.strictEqual(underflowBigSt.mtimeNs, 0n);
  assert.strictEqual(underflowBigSt.ctimeNs, 0n);
  assert.strictEqual(underflowBigSt.birthtimeNs, 0n);
}

// Directories and symlinks share the same conversion.
{
  const dirSt = createDirectoryStats({
    atimeMs: -500,
    mtimeMs: -1500,
  });
  assert.strictEqual(dirSt.atimeMs, -500);
  assert.strictEqual(dirSt.mtimeMs, -1500);

  const dirBigSt = createDirectoryStats({
    atimeMs: -500,
    mtimeMs: -0.0000001,
    bigint: true,
  });
  assert.strictEqual(dirBigSt.atimeNs, -500_000_000n);
  assert.strictEqual(dirBigSt.mtimeNs, 0n);

  const linkSt = createSymlinkStats(0, {
    atimeMs: -1500,
    mtimeMs: -500,
  });
  assert.strictEqual(linkSt.atimeMs, -1500);
  assert.strictEqual(linkSt.mtimeMs, -500);

  const linkBigSt = createSymlinkStats(0, {
    atimeMs: -1500,
    mtimeMs: -1000.0000001,
    bigint: true,
  });
  assert.strictEqual(linkBigSt.atimeNs, -1_500_000_000n);
  assert.strictEqual(linkBigSt.mtimeNs, -1_000_000_000n);
}

// Millisecond values that are not exactly representable (123.4 is stored as
// 123.39999999999999) must be rounded to whole nanoseconds instead of making
// BigInt() throw.
{
  const bigSt = createFileStats(0, {
    atimeMs: 123.39999999999999,
    mtimeMs: -123.39999999999999,
    ctimeMs: 1234.5678,
    birthtimeMs: 1700000000123.5,
    bigint: true,
  });
  assert.strictEqual(bigSt.atimeNs, 123_400_000n);
  assert.strictEqual(bigSt.mtimeNs, -123_400_000n);
  assert.strictEqual(bigSt.ctimeNs, 1_234_567_800n);
  assert.strictEqual(bigSt.birthtimeNs, 1_700_000_000_123_500_000n);
}

// The seconds and nanoseconds reported to Stats must follow the file system
// convention (seconds rounded down, nanoseconds in [0, 1e9)), otherwise the
// Temporal.Instant values derived from them would be off by a second.
if (common.hasTemporal) {
  const st = createFileStats(0, {
    atimeMs: -500,
    mtimeMs: -1500.5,
    ctimeMs: 1500.5,
    birthtimeMs: -463.2,
  });
  assert.strictEqual(st.atimeInstant.epochNanoseconds, -500_000_000n);
  assert.strictEqual(st.mtimeInstant.epochNanoseconds, -1_500_500_000n);
  assert.strictEqual(st.ctimeInstant.epochNanoseconds, 1_500_500_000n);
  assert.strictEqual(st.birthtimeInstant.epochNanoseconds, -463_200_000n);

  const bigSt = createFileStats(0, {
    atimeMs: -500,
    mtimeMs: -1500.5,
    ctimeMs: 1500.5,
    birthtimeMs: -463.2,
    bigint: true,
  });
  assert.strictEqual(bigSt.atimeInstant.epochNanoseconds, -500_000_000n);
  assert.strictEqual(bigSt.mtimeInstant.epochNanoseconds, -1_500_500_000n);
  assert.strictEqual(bigSt.ctimeInstant.epochNanoseconds, 1_500_500_000n);
  assert.strictEqual(bigSt.birthtimeInstant.epochNanoseconds, -463_200_000n);
}
