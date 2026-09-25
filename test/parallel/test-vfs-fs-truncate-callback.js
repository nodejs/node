// Flags: --experimental-vfs
'use strict';

// fs.truncate, fs.link, and fs.mkdtemp callbacks dispatch through VFS.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.mkdirSync('/src', { recursive: true });
myVfs.writeFileSync('/src/hello.txt', 'hello world');
const mountPoint = myVfs.mount();

fs.truncate(path.join(mountPoint, 'src/hello.txt'), 5,
            common.mustSucceed(() => {
              assert.strictEqual(
                fs.readFileSync(path.join(mountPoint, 'src/hello.txt'),
                                'utf8'),
                'hello',
              );

              fs.link(path.join(mountPoint, 'src/hello.txt'),
                      path.join(mountPoint, 'src/lk.txt'),
                      common.mustSucceed(() => {
                        assert.strictEqual(
                          fs.readFileSync(path.join(mountPoint, 'src/lk.txt'),
                                          'utf8'),
                          'hello',
                        );

                        fs.mkdtemp(path.join(mountPoint, 'src/td-'),
                                   common.mustSucceed((dir) => {
                                     assert.ok(dir.startsWith(
                                       path.join(mountPoint, 'src/td-')));
                                     assert.strictEqual(
                                       fs.statSync(dir).isDirectory(), true,
                                     );
                                     myVfs.unmount();
                                   }));
                      }));
            }));
