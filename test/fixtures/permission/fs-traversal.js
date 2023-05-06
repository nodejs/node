'use strict'

const common = require('../../common');

const assert = require('assert');
const fs = require('fs');
const path = require('path');

const blockedFolder = process.env.BLOCKEDFOLDER;
const allowedFolder = process.env.ALLOWEDFOLDER;
const traversalPath = allowedFolder + '../file.md'

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
    resource: path.toNamespacedPath(path.resolve(traversalPath)),
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
    resource: path.toNamespacedPath(path.resolve(traversalPath)),
  }));
}

{
  assert.ok(!process.permission.has('fs.read', traversalPath));
  assert.ok(!process.permission.has('fs.write', traversalPath));
}