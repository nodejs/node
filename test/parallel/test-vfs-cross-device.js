'use strict';

// Cross-VFS rename, copyFile, and link must throw EXDEV.

require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

const vfs1 = vfs.create();
vfs1.writeFileSync('/src.txt', 'data');
vfs1.mount('/xdev_a');

const vfs2 = vfs.create();
vfs2.mkdirSync('/dest');
vfs2.mount('/xdev_b');

assert.throws(() => fs.renameSync('/xdev_a/src.txt', '/xdev_b/dest/src.txt'), {
  code: 'EXDEV',
});

assert.throws(() => fs.copyFileSync('/xdev_a/src.txt', '/xdev_b/dest/src.txt'), {
  code: 'EXDEV',
});

assert.throws(() => fs.linkSync('/xdev_a/src.txt', '/xdev_b/dest/link.txt'), {
  code: 'EXDEV',
});

vfs2.unmount();
vfs1.unmount();
