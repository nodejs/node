// Flags: --experimental-vfs --expose-internals
'use strict';

// MemoryFileHandle internals: the "stats not available" path when there
// is no entry/getStats callback wired up.

require('../common');
const assert = require('assert');
const { MemoryFileHandle } = require('internal/vfs/file_handle');

// MemoryFileHandle without a #getStats callback throws ERR_INVALID_STATE
{
  const h = new MemoryFileHandle('/x', 'r', 0o644, Buffer.alloc(0), null, undefined);
  assert.throws(() => h.statSync(), { code: 'ERR_INVALID_STATE' });
}
