'use strict';

// Cover branch paths in provider.js — explicit options.flag / options.mode
// for writeFile/appendFile, and the access-mode permission denials.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const vfs = require('node:vfs');

const { R_OK, W_OK, X_OK } = fs.constants;

// writeFile / writeFileSync with explicit flag and mode
{
  const myVfs = vfs.create();
  myVfs.writeFileSync('/a.txt', 'hello', { flag: 'w', mode: 0o600 });
  assert.strictEqual(myVfs.readFileSync('/a.txt', 'utf8'), 'hello');

  // promises path
  myVfs.promises.writeFile('/b.txt', 'world', { flag: 'w', mode: 0o600 })
    .then(common.mustCall(() => {
      assert.strictEqual(myVfs.readFileSync('/b.txt', 'utf8'), 'world');
    }));
}

// appendFile / appendFileSync with explicit flag and mode
{
  const myVfs = vfs.create();
  myVfs.appendFileSync('/a.txt', 'first', { flag: 'a', mode: 0o600 });
  myVfs.appendFileSync('/a.txt', '-second', { flag: 'a' });
  assert.strictEqual(myVfs.readFileSync('/a.txt', 'utf8'), 'first-second');

  myVfs.promises.appendFile('/b.txt', 'go', { flag: 'a', mode: 0o600 })
    .then(common.mustCall(() => {
      assert.strictEqual(myVfs.readFileSync('/b.txt', 'utf8'), 'go');
    }));
}

// access permission denials — chmod the file to a permission-restricted mode
// so that R_OK / W_OK / X_OK each trigger EACCES via #checkAccessMode.
{
  const myVfs = vfs.create();

  // No read permission (mode = 0o222 → owner has W only)
  myVfs.writeFileSync('/no-r.txt', 'x');
  myVfs.chmodSync('/no-r.txt', 0o222);
  assert.throws(() => myVfs.accessSync('/no-r.txt', R_OK), { code: 'EACCES' });
  assert.rejects(myVfs.promises.access('/no-r.txt', R_OK),
                 { code: 'EACCES' }).then(common.mustCall());

  // No write permission (mode = 0o444 → owner has R only)
  myVfs.writeFileSync('/no-w.txt', 'x');
  myVfs.chmodSync('/no-w.txt', 0o444);
  assert.throws(() => myVfs.accessSync('/no-w.txt', W_OK), { code: 'EACCES' });
  assert.rejects(myVfs.promises.access('/no-w.txt', W_OK),
                 { code: 'EACCES' }).then(common.mustCall());

  // No execute permission (mode = 0o644)
  myVfs.writeFileSync('/no-x.txt', 'x');
  myVfs.chmodSync('/no-x.txt', 0o644);
  assert.throws(() => myVfs.accessSync('/no-x.txt', X_OK), { code: 'EACCES' });
  assert.rejects(myVfs.promises.access('/no-x.txt', X_OK),
                 { code: 'EACCES' }).then(common.mustCall());

  // F_OK (mode 0) — existence-only check, no permission needed
  myVfs.accessSync('/no-r.txt', 0);
  // mode passed as null also exits early
  myVfs.accessSync('/no-r.txt', null);
}
