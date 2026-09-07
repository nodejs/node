// Flags: --experimental-vfs
'use strict';

// fs.open / fs.fstat / fs.read / fs.write / fs.close / fs.ftruncate callbacks
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

// Open + fstat + read + close
{
  const { myVfs, mountPoint } = mounted();
  fs.open(path.join(mountPoint, 'src/hello.txt'), 'r',
          common.mustSucceed((fd) => {
            assert.notStrictEqual(fd & 0x40000000, 0);
            fs.fstat(fd, common.mustSucceed((stats) => {
              assert.strictEqual(stats.isFile(), true);
              const buf = Buffer.alloc(11);
              fs.read(fd, buf, 0, 11, 0,
                      common.mustSucceed((n, buffer) => {
                        assert.strictEqual(n, 11);
                        assert.strictEqual(buffer.toString(), 'hello world');
                        fs.close(fd,
                                 common.mustSucceed(() => myVfs.unmount()));
                      }));
            }));
          }));
}

// Open + write + close
{
  const { myVfs, mountPoint } = mounted();
  fs.open(path.join(mountPoint, 'src/aw.txt'), 'w',
          common.mustSucceed((fd) => {
            const data = Buffer.from('async-fd');
            fs.write(fd, data, 0, data.length, 0,
                     common.mustSucceed((n) => {
                       assert.strictEqual(n, data.length);
                       fs.close(fd, common.mustSucceed(() => {
                         assert.strictEqual(
                           fs.readFileSync(
                             path.join(mountPoint, 'src/aw.txt'), 'utf8'),
                           'async-fd',
                         );
                         myVfs.unmount();
                       }));
                     }));
          }));
}

// ftruncate (cb)
{
  const { myVfs, mountPoint } = mounted();
  const fd = fs.openSync(path.join(mountPoint, 'src/hello.txt'), 'r+');
  fs.ftruncate(fd, 5, common.mustSucceed(() => {
    fs.closeSync(fd);
    assert.strictEqual(
      fs.readFileSync(path.join(mountPoint, 'src/hello.txt'), 'utf8'),
      'hello',
    );
    myVfs.unmount();
  }));
}
