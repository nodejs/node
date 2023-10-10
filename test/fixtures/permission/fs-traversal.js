'use strict'

const common = require('../../common');

const assert = require('assert');
const fs = require('fs');
const path = require('path');

// This should not affect how the permission model resolves paths.
const { resolve } = path;
path.resolve = (s) => s;

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

{
  assert.ok(!process.permission.has('fs.read', traversalPath));
  assert.ok(!process.permission.has('fs.write', traversalPath));
  assert.ok(!process.permission.has('fs.read', traversalFolderPath));
  assert.ok(!process.permission.has('fs.write', traversalFolderPath));
}
