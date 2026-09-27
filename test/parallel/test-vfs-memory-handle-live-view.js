// Flags: --experimental-vfs
'use strict';
// MemoryProvider: an open handle is a live view of the file, not a snapshot
// taken at open time. Writes made through the provider or through another
// handle must be visible to handles that are already open, as they are on a
// real filesystem. Refs: https://github.com/nodejs/node/issues/66355
const common = require('../common');
const assert = require('assert');
const { create } = require('node:vfs');

// Content replaced through the provider is seen by an open read handle.
{
  const { provider: p } = create();
  p.writeFileSync('/f', 'AAAA');
  const h = p.openSync('/f', 'r');
  p.writeFileSync('/f', '0123456789');

  const b = Buffer.alloc(10);
  assert.strictEqual(h.readSync(b, 0, 10, 0), 10);
  assert.strictEqual(b.toString(), '0123456789');
  assert.strictEqual(h.readFileSync('utf8'), '0123456789');
  assert.strictEqual(h.statSync().size, 10);
  assert.strictEqual(p.statSync('/f').size, 10);
  h.readFile('utf8').then(common.mustCall((s) => {
    assert.strictEqual(s, '0123456789');
    h.closeSync();
  }));
}

// A shrinking rewrite is seen too: reads stop at the new end.
{
  const { provider: p } = create();
  p.writeFileSync('/f', '0123456789');
  const h = p.openSync('/f', 'r');
  p.writeFileSync('/f', 'AB');

  const b = Buffer.alloc(10);
  assert.strictEqual(h.readSync(b, 0, 10, 0), 2);
  assert.strictEqual(b.subarray(0, 2).toString(), 'AB');
  assert.strictEqual(h.readFileSync('utf8'), 'AB');
  assert.strictEqual(h.statSync().size, 2);
  h.closeSync();
}

// Two writable handles on the same file do not clobber each other.
{
  const { provider: p } = create();
  p.writeFileSync('/f', 'AAAA');
  const h1 = p.openSync('/f', 'r+');
  const h2 = p.openSync('/f', 'r+');

  h1.writeSync(Buffer.from('XY'), 0, 2, 0);
  h2.writeSync(Buffer.from('Z'), 0, 1, 3);
  assert.strictEqual(p.readFileSync('/f', 'utf8'), 'XYAZ');
  assert.strictEqual(h1.readFileSync('utf8'), 'XYAZ');

  // Growing through one handle is visible to the other.
  h1.writeSync(Buffer.from('0123456789'), 0, 10, 4);
  assert.strictEqual(h2.statSync().size, 14);
  assert.strictEqual(h2.readFileSync('utf8'), 'XYAZ0123456789');

  // Truncating through one handle is visible to the other.
  h2.truncateSync(3);
  assert.strictEqual(h1.statSync().size, 3);
  assert.strictEqual(h1.readFileSync('utf8'), 'XYA');

  h1.closeSync();
  h2.closeSync();
}

// A write through a handle opened before the file was replaced lands in the
// current content instead of resurrecting the old bytes.
{
  const { provider: p } = create();
  p.writeFileSync('/f', 'AAAA');
  const h = p.openSync('/f', 'r+');
  p.writeFileSync('/f', 'BB');

  h.writeSync(Buffer.from('C'), 0, 1, 3);
  assert.deepStrictEqual(p.readFileSync('/f'), Buffer.from('BB\0C', 'latin1'));
  h.closeSync();
}

// An append handle opened earlier appends after the current end.
{
  const { provider: p } = create();
  p.writeFileSync('/f', 'AAAA');
  const h = p.openSync('/f', 'a');
  p.writeFileSync('/f', 'B');

  h.writeSync(Buffer.from('C'), 0, 1, null);
  assert.strictEqual(p.readFileSync('/f', 'utf8'), 'BC');
  h.closeSync();
}
