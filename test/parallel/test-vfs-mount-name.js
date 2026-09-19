// Flags: --experimental-vfs
'use strict';

require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const vfs = require('node:vfs');
const { spawnSyncAndAssert } = require('../common/child_process');

const root = path.join(os.devNull, 'vfs');

// A name must be a single path segment that cannot be taken for a layer id.
{
  const myVfs = vfs.create();
  for (const name of ['', '.', '..', '0', '17', '12345', '100000000000000000000',
                      'a/b', 'a\\b', 'a\0b']) {
    assert.throws(() => myVfs.mount(name), { code: 'ERR_INVALID_ARG_VALUE' },
                  JSON.stringify(name));
  }
  for (const name of [null, 1, {}, Symbol('name')]) {
    assert.throws(() => myVfs.mount(name), { code: 'ERR_INVALID_ARG_TYPE' });
  }
  // A rejected name leaves the file system unmounted.
  assert.strictEqual(myVfs.mounted, false);

  // Anything else is a name, including names special on ordinary objects.
  for (const name of ['a', '0a', 'a0', '00', '07', '-0', '-1', '1.5', '1e3', ' 1',
                      '+1', 'NaN', 'Infinity', '-Infinity', '...', '__proto__',
                      'constructor', 'π']) {
    myVfs.mount(name);
    assert.strictEqual(fs.readlinkSync(path.join(root, name)),
                       path.basename(myVfs.mountPoint), name);
    myVfs.unmount();
  }
}

// Mounting a mounted file system under a name fails without taking the name.
{
  const first = vfs.create();
  const second = vfs.create();
  first.mount('taken');
  second.mount();
  assert.throws(() => second.mount('taken'), { code: 'ERR_INVALID_STATE' });
  assert.strictEqual(fs.realpathSync(path.join(root, 'taken')), first.mountPoint);
  first.unmount();
  second.unmount();
}

// --vfs-mount and --vfs-load take an optional `name=` prefix.
{
  tmpdir.refresh();
  const plain = tmpdir.resolve('plain');
  const withEq = tmpdir.resolve('x=y');
  fs.mkdirSync(plain);
  fs.mkdirSync(withEq);
  fs.writeFileSync(path.join(plain, 'id.txt'), 'plain');
  fs.writeFileSync(path.join(withEq, 'id.txt'), 'x=y');
  fs.writeFileSync(path.join(plain, 'index.js'), `
const fs = require('fs');
const path = require('path');
const link = path.join(require('os').devNull, 'vfs', 'app');
console.log(JSON.stringify([process.argv[1], fs.realpathSync(link) === __dirname]));
`);

  // Reports the number of mounts, and the mount each name leads to.
  const report = `
const fs = require('fs');
const path = require('path');
const root = path.join(require('os').devNull, 'vfs');
const names = {};
let layers = 0;
for (const entry of fs.readdirSync(root, { withFileTypes: true })) {
  if (entry.isSymbolicLink()) {
    names[entry.name] = fs.readFileSync(path.join(root, entry.name, 'id.txt'), 'utf8');
  } else {
    layers++;
  }
}
console.log(JSON.stringify([layers, names]));
`;

  function mounts(values, layers, names) {
    spawnSyncAndAssert(process.execPath, [
      '--experimental-vfs', '--no-warnings',
      ...values.map((v) => `--vfs-mount=${v}`),
      '-e', report,
    ], { cwd: tmpdir.path }, {
      stdout(output) {
        assert.deepStrictEqual(JSON.parse(output), [layers, names]);
      },
    });
  }

  mounts([`assets=${plain}`], 1, { assets: 'plain' });
  mounts([plain], 1, {});
  // Only the first `=` separates the name.
  mounts([`a=${withEq}`], 1, { a: 'x=y' });
  // A prefix holding a path separator belongs to the source.
  mounts([withEq], 1, {});
  mounts(['./x=y'], 1, {});
  // An empty name is no name.
  mounts([`=${plain}`], 1, {});
  // Relative sources resolve after the name is removed.
  mounts(['n=plain'], 1, { n: 'plain' });
  mounts([`one=${plain}`, `two=${withEq}`], 2, { one: 'plain', two: 'x=y' });
  // A later mount takes the name over.
  mounts([`dup=${plain}`, `dup=${withEq}`], 2, { dup: 'x=y' });

  // A name the file system cannot be mounted under is an error.
  spawnSyncAndAssert(process.execPath, [
    '--experimental-vfs', '--no-warnings', `--vfs-mount=7=${plain}`, '-e', '',
  ], {
    status: 1,
    stderr: /ERR_INVALID_ARG_VALUE/,
  });

  // --vfs-load names its mount the same way, and process.argv[1] reports
  // the source without the name.
  spawnSyncAndAssert(process.execPath, [
    '--experimental-vfs', '--no-warnings', `--vfs-load=app=${plain}`,
  ], {
    stdout: JSON.stringify([plain, true]),
    trim: true,
  });
}
