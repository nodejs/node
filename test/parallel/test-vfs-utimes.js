// Flags: --disable-warning=ExperimentalWarning
'use strict';

// utimes / lutimes accept Date instances, numeric seconds, strings,
// and other values (which fall through to a no-op time value).

require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/u.txt', 'x');
myVfs.symlinkSync('/u.txt', '/lk');

// Numeric seconds branch
myVfs.utimesSync('/u.txt', 1000, 2000);

// Date object branch
myVfs.utimesSync('/u.txt', new Date(3000000), new Date(4000000));

// String time → DateNow() fallback
myVfs.utimesSync('/u.txt', 'now', 'now');

// null/undefined → fallthrough (returns the value as-is)
myVfs.utimesSync('/u.txt', null, undefined);

// lutimes for symlinks
myVfs.lutimesSync('/lk', new Date(0), new Date(0));

// Times before the epoch and fractional seconds are reported back unchanged,
// both as Stats and as BigIntStats.
{
  const v = vfs.create();
  v.writeFileSync('/neg.txt', 'x');

  v.utimesSync('/neg.txt', -1.5, -0.5);
  let st = v.statSync('/neg.txt');
  assert.strictEqual(st.atimeMs, -1500);
  assert.strictEqual(st.mtimeMs, -500);
  assert.strictEqual(st.atime.getTime(), -1500);
  assert.strictEqual(st.mtime.getTime(), -500);

  let bigSt = v.statSync('/neg.txt', { bigint: true });
  assert.strictEqual(bigSt.atimeNs, -1_500_000_000n);
  assert.strictEqual(bigSt.mtimeNs, -500_000_000n);

  // 0.1234 * 1000 is 123.39999999999999, which is not a whole number of ns.
  v.utimesSync('/neg.txt', 0.1234, -0.1234);
  st = v.statSync('/neg.txt');
  assert.strictEqual(st.atime.getTime(), 123);
  assert.strictEqual(st.mtime.getTime(), -123);
  bigSt = v.statSync('/neg.txt', { bigint: true });
  assert.strictEqual(bigSt.atimeNs, 123_400_000n);
  assert.strictEqual(bigSt.mtimeNs, -123_400_000n);
}
