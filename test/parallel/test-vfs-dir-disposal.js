'use strict';

// VirtualDir must support Symbol.dispose and Symbol.asyncDispose.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mkdirSync('/dir');
myVfs.writeFileSync('/dir/a.txt', 'a');
myVfs.mount('/mnt_dirdispose');

// Symbol.dispose via closeSync
{
  const dir = fs.opendirSync('/mnt_dirdispose/dir');
  dir[Symbol.dispose]();
  assert.throws(() => dir.closeSync(), { code: 'ERR_DIR_CLOSED' });
}

// Symbol.asyncDispose via close
{
  const dir = fs.opendirSync('/mnt_dirdispose/dir');
  dir[Symbol.asyncDispose]().then(common.mustCall(() => {
    assert.throws(() => dir.closeSync(), { code: 'ERR_DIR_CLOSED' });
  }));
}

myVfs.unmount();
