'use strict';

// fs.promises.watch() abort signal must reject with AbortError, not resolve done.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

const myVfs = vfs.create();
myVfs.writeFileSync('/file.txt', 'x');
myVfs.mount('/mnt_wab');

const ac = new AbortController();
const iter = fs.promises.watch('/mnt_wab/file.txt', { signal: ac.signal });
const pending = iter.next();
ac.abort();

pending.then(
  () => { throw new Error('Expected rejection'); },
  common.mustCall((err) => {
    assert.strictEqual(err.name, 'AbortError');
    myVfs.unmount();
  }),
);
