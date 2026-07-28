// Flags: --experimental-vfs
'use strict';

// Constructor variants and option validation for vfs.create() and
// `new VirtualFileSystem(...)`.

require('../common');
const assert = require('assert');
const vfs = require('node:vfs');

// vfs.create() with no arguments uses the default MemoryProvider
{
  const myVfs = vfs.create();
  assert.ok(myVfs.provider instanceof vfs.MemoryProvider);
}

// vfs.create with first arg as options object (no provider)
{
  const myVfs = vfs.create({ emitExperimentalWarning: false });
  assert.ok(myVfs);
  assert.ok(myVfs.provider instanceof vfs.MemoryProvider);
}

// vfs.create with explicit provider
{
  const provider = new vfs.MemoryProvider();
  const myVfs = vfs.create(provider);
  assert.strictEqual(myVfs.provider, provider);
}

// new VirtualFileSystem(options) directly
{
  const myVfs = new vfs.VirtualFileSystem({ emitExperimentalWarning: false });
  assert.ok(myVfs);
}

// emitExperimentalWarning option must be a boolean
{
  assert.throws(
    () => new vfs.VirtualFileSystem({ emitExperimentalWarning: 'not-bool' }),
    { code: 'ERR_INVALID_ARG_TYPE' });
}

// existsSync swallows ALL errors from the provider, not just ENOENT
{
  class ThrowingProvider extends vfs.VirtualProvider {
    existsSync() { throw new Error('boom'); }
  }
  const myVfs = vfs.create(new ThrowingProvider());
  assert.strictEqual(myVfs.existsSync('/anything'), false);
}

// Walking a path through a regular-file parent throws ENOTDIR
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/file.txt', 'x');
  assert.throws(() => myVfs.writeFileSync('/file.txt/oops', 'y'),
                { code: 'ENOTDIR' });
}

// statSync('/') returns the root directory
{
  const myVfs = vfs.create();
  assert.ok(myVfs.statSync('/').isDirectory());
}
