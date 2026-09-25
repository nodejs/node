// Flags: --experimental-vfs
'use strict';

// access / accessSync honour the R_OK / W_OK / X_OK / F_OK mode bits and
// throw EACCES when the file's permission bits don't allow the request.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

const { R_OK, W_OK, X_OK } = fs.constants;

const myVfs = vfs.create();

// No read permission (mode 0o222 → owner has W only)
myVfs.writeFileSync('/no-r.txt', 'x');
myVfs.chmodSync('/no-r.txt', 0o222);
assert.throws(() => myVfs.accessSync('/no-r.txt', R_OK), { code: 'EACCES' });
assert.rejects(myVfs.promises.access('/no-r.txt', R_OK),
               { code: 'EACCES' }).then(common.mustCall());

// No write permission (mode 0o444 → owner has R only)
myVfs.writeFileSync('/no-w.txt', 'x');
myVfs.chmodSync('/no-w.txt', 0o444);
assert.throws(() => myVfs.accessSync('/no-w.txt', W_OK), { code: 'EACCES' });
assert.rejects(myVfs.promises.access('/no-w.txt', W_OK),
               { code: 'EACCES' }).then(common.mustCall());

// No execute permission (mode 0o644)
myVfs.writeFileSync('/no-x.txt', 'x');
myVfs.chmodSync('/no-x.txt', 0o644);
assert.throws(() => myVfs.accessSync('/no-x.txt', X_OK), { code: 'EACCES' });
assert.rejects(myVfs.promises.access('/no-x.txt', X_OK),
               { code: 'EACCES' }).then(common.mustCall());

// F_OK (mode 0) is an existence-only check and does not require permission
myVfs.accessSync('/no-r.txt', 0);

// Mode passed as null also exits early (existence-only)
myVfs.accessSync('/no-r.txt', null);
