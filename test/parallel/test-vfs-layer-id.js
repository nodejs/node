// Flags: --experimental-vfs
'use strict';

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

  const idBefore = a.layerId;
  a.mount();
  assert.strictEqual(a.layerId, idBefore);
  a.unmount();
  assert.strictEqual(a.layerId, idBefore);
}

// layerId is the layer segment of the mount point.
{
  const v = vfs.create();
  const mountPoint = v.mount();
  assert.strictEqual(
    mountPoint,
    path.join(os.devNull, 'vfs', String(v.layerId)));
  v.unmount();
}
