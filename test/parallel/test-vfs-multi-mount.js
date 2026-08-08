// Flags: --experimental-vfs
'use strict';

// Two concurrent mounts must each route to its own VFS without
// interference. Also exercises that the layer registry routes
// correctly when more than one VFS is active.

require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vfs = require('node:vfs');

const a = vfs.create();
a.writeFileSync('/file.txt', 'from-a');
a.mkdirSync('/dir', { recursive: true });
a.writeFileSync('/dir/inside.txt', 'a-inside');

const b = vfs.create();
b.writeFileSync('/file.txt', 'from-b');
b.mkdirSync('/dir', { recursive: true });
b.writeFileSync('/dir/inside.txt', 'b-inside');

const baseA = a.mount();
const baseB = b.mount();

// Each mount sees its own content
assert.strictEqual(fs.readFileSync(path.join(baseA, 'file.txt'), 'utf8'),
                   'from-a');
assert.strictEqual(fs.readFileSync(path.join(baseB, 'file.txt'), 'utf8'),
                   'from-b');

// Per-mount directory listings are isolated
assert.deepStrictEqual(
  fs.readdirSync(baseA).sort(),
  ['dir', 'file.txt'],
);
assert.deepStrictEqual(
  fs.readdirSync(baseB).sort(),
  ['dir', 'file.txt'],
);

// Writing to one mount doesn't bleed into the other
fs.writeFileSync(path.join(baseA, 'only-a.txt'), 'A');
assert.strictEqual(fs.existsSync(path.join(baseB, 'only-a.txt')), false);
assert.strictEqual(fs.readFileSync(path.join(baseA, 'only-a.txt'), 'utf8'),
                   'A');

// realpathSync returns the mount-rooted path (proves #toMountedPath on each)
assert.strictEqual(fs.realpathSync(path.join(baseA, 'file.txt')),
                   path.join(baseA, 'file.txt'));
assert.strictEqual(fs.realpathSync(path.join(baseB, 'file.txt')),
                   path.join(baseB, 'file.txt'));

// Unmount one, the other still works
a.unmount();
assert.strictEqual(a.mounted, false);
assert.strictEqual(b.mounted, true);
assert.strictEqual(fs.readFileSync(path.join(baseB, 'file.txt'), 'utf8'),
                   'from-b');

b.unmount();
