'use strict'

const common = require('../../common');

const assert = require('assert');
const fs = require('fs');
const path = require('path');

const { resolve } = path;
// This should not affect how the permission model resolves paths.
try {
  path.resolve = (s) => s;
  assert.fail('should not be called');
} catch {}

const blockedFolder = process.env.BLOCKEDFOLDER;
const allowedFolder = process.env.ALLOWEDFOLDER;
const traversalPath = allowedFolder + '/../file.md';
const traversalFolderPath = allowedFolder + '/../folder';
const bufferTraversalPath = Buffer.from(traversalPath);
const uint8ArrayTraversalPath = new TextEncoder().encode(traversalPath);

{
  assert.ok(process.permission.has('fs.read', allowedFolder));
  assert.ok(process.permission.has('fs.write', allowedFolder));
  assert.ok(!process.permission.has('fs.read', blockedFolder));
  assert.ok(!process.permission.has('fs.write', blockedFolder));
}

{
  assert.throws(() => {
    fs.writeFile(traversalPath, 'test', (error) => {
      assert.ifError(error);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: path.toNamespacedPath(resolve(traversalPath)),
  }));
}

{
  assert.throws(() => {
    fs.readFile(traversalPath, (error) => {
      assert.ifError(error);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: path.toNamespacedPath(resolve(traversalPath)),
  }));
}

{
  assert.throws(() => {
    fs.mkdtempSync(traversalFolderPath, (error) => {
      assert.ifError(error);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: resolve(traversalFolderPath + 'XXXXXX'),
  }));
}

{
  assert.throws(() => {
    fs.mkdtemp(traversalFolderPath, (error) => {
      assert.ifError(error);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemWrite',
    resource: resolve(traversalFolderPath + 'XXXXXX'),
  }));
}

{
  assert.throws(() => {
    fs.readFile(bufferTraversalPath, (error) => {
      assert.ifError(error);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: resolve(traversalPath),
  }));
}

{
  assert.throws(() => {
    fs.readFile(uint8ArrayTraversalPath, (error) => {
      assert.ifError(error);
    });
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: resolve(traversalPath),
  }));
}

// Monkey-patching path module should also not allow path traversal.
{
  const fs = require('fs');
  const path = require('path');

  const cwd = Buffer.from('.');
  try {
    path.toNamespacedPath = (path) => { return traversalPath; };
    assert.fail('should throw error when pacthing');
  } catch { }

  assert.throws(() => {
    fs.readFile(cwd, common.mustNotCall());
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: resolve(cwd.toString()),
  }));
}

// Monkey-patching Buffer internals should also not allow path traversal.
{
  const extraChars = '.'.repeat(40);
  const traversalPathWithExtraChars = traversalPath + extraChars;
  const traversalPathWithExtraBytes = Buffer.from(traversalPathWithExtraChars);

  Buffer.prototype.utf8Write = ((w) => function(str, ...args) {
    assert.strictEqual(str, resolve(traversalPath) + extraChars);
    return w.apply(this, [traversalPath, ...args]);
  })(Buffer.prototype.utf8Write);

  // Sanity check (remove if the internals of Buffer.from change):
  // The custom implementation of utf8Write should cause Buffer.from() to encode
  // traversalPath instead of the sanitized output of resolve().
  assert.strictEqual(Buffer.from(resolve(traversalPathWithExtraChars)).toString(), traversalPath);

  assert.throws(() => {
    fs.readFile(traversalPathWithExtraBytes, common.mustNotCall());
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: resolve(traversalPathWithExtraChars),
  }));

  assert.throws(() => {
    fs.readFile(new TextEncoder().encode(traversalPathWithExtraBytes.toString()), common.mustNotCall());
  }, common.expectsError({
    code: 'ERR_ACCESS_DENIED',
    permission: 'FileSystemRead',
    resource: resolve(traversalPathWithExtraChars),
  }));
}

{
  assert.ok(!process.permission.has('fs.read', traversalPath));
  assert.ok(!process.permission.has('fs.write', traversalPath));
  assert.ok(!process.permission.has('fs.read', traversalFolderPath));
  assert.ok(!process.permission.has('fs.write', traversalFolderPath));
}
