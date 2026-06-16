// Flags: --experimental-vfs
'use strict';

// `vfs.layerId` is a per-process monotonically increasing identifier
// assigned at construction. It is stable across mount/unmount cycles
// and surfaces in overlap error messages.

require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

{
  const a = vfs.create();
  const b = vfs.create();
  const c = vfs.create();
  assert.strictEqual(typeof a.layerId, 'number');
  assert.ok(b.layerId > a.layerId);
  assert.ok(c.layerId > b.layerId);
  // Stable across mount/unmount.
  const idBefore = a.layerId;
  a.mount('/mnt-layer-stable');
  assert.strictEqual(a.layerId, idBefore);
  a.unmount();
  assert.strictEqual(a.layerId, idBefore);
}

// layerId appears in the overlap error message so the user can tell
// which instance lost the race.
{
  const outer = vfs.create();
  const inner = vfs.create();
  outer.mount('/mnt-layer-overlap');
  assert.throws(
    () => inner.mount('/mnt-layer-overlap/sub'),
    (err) => {
      assert.strictEqual(err.code, 'ERR_INVALID_STATE');
      assert.match(err.message, new RegExp(`layer ${inner.layerId}`));
      assert.match(err.message, new RegExp(`layer ${outer.layerId}`));
      return true;
    },
  );
  outer.unmount();
}
