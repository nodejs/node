// Flags: --experimental-vfs
'use strict';

// `vfs.layerId` is a per-process monotonically increasing identifier
// assigned at construction. It is stable across mount/unmount cycles
// and forms the layer segment of the reserved mount namespace.

require('../common');
const assert = require('assert');
const os = require('os');
const path = require('path');
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
  a.mount();
  assert.strictEqual(a.layerId, idBefore);
  a.unmount();
  assert.strictEqual(a.layerId, idBefore);
}

// layerId is the layer segment of the mount point, so the owning layer
// of any VFS path is visible in the path itself.
{
  const v = vfs.create();
  const mountPoint = v.mount();
  assert.strictEqual(
    mountPoint,
    path.join(os.devNull, 'vfs', String(v.layerId)));
  v.unmount();
}
