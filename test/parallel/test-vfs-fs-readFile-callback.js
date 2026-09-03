// Flags: --experimental-vfs
'use strict';

// fs.readFile, fs.readdir, fs.realpath, fs.access, and fs.exists callbacks
// dispatch through VFS.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

function mounted() {
  const myVfs = vfs.create();
  myVfs.mkdirSync('/src', { recursive: true });
  myVfs.writeFileSync('/src/hello.txt', 'hello world');
  const mountPoint = myVfs.mount();
  return { myVfs, mountPoint };
}

// readFile (cb)
{
  const { myVfs, mountPoint } = mounted();
  fs.readFile(path.join(mountPoint, 'src/hello.txt'), 'utf8',
              common.mustSucceed((data) => {
                assert.strictEqual(data, 'hello world');
                myVfs.unmount();
              }));
}

// readdir (cb)
{
  const { myVfs, mountPoint } = mounted();
  fs.readdir(path.join(mountPoint, 'src'),
             common.mustSucceed((entries) => {
               assert.ok(entries.includes('hello.txt'));
               myVfs.unmount();
             }));
}

// realpath (cb)
{
  const { myVfs, mountPoint } = mounted();
  fs.realpath(path.join(mountPoint, 'src/hello.txt'),
              common.mustSucceed((rp) => {
                assert.strictEqual(rp, path.join(mountPoint, 'src/hello.txt'));
                myVfs.unmount();
              }));
}

// access (cb)
{
  const { myVfs, mountPoint } = mounted();
  fs.access(path.join(mountPoint, 'src/hello.txt'),
            common.mustSucceed(() => {
              myVfs.unmount();
            }));
}

// exists (cb) - signature is (exists) not (err, exists), use mustCall
{
  const { myVfs, mountPoint } = mounted();
  fs.exists(path.join(mountPoint, 'src/hello.txt'),
            common.mustCall((ok) => {
              assert.strictEqual(ok, true);
              fs.exists(path.join(mountPoint, 'missing'),
                        common.mustCall((ok2) => {
                          assert.strictEqual(ok2, false);
                          myVfs.unmount();
                        }));
            }));
}
