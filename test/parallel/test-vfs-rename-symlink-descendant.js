// Flags: --experimental-vfs
'use strict';

const common = require('../common');
const assert = require('assert');
const { create } = require('node:vfs');

async function test() {
  for (const async of [false, true]) {
    for (const [oldPath, newPath, target] of [
      ['/a', '/alias/moved', '/a/b'],
      ['/a', '/alias/moved', 'a/b'],
      ['/a', '/alias/moved', '/source/a/b'],
      ['/a', '/alias/moved', '/a'],
      ['/a', '/alias/b/moved', '/a'],
      ['/source/a', '/alias/moved', '/a/b'],
      ['/source/a', '/a/b/moved', '/a/b'],
      ['/a', '/alias/keep.txt', '/a'],
    ]) {
      const fs = create();
      fs.mkdirSync('/a/b', { recursive: true });
      fs.writeFileSync('/a/keep.txt', 'keep');
      fs.symlinkSync(target, '/alias');
      fs.symlinkSync('/', '/source');

      const error = { code: 'EINVAL', syscall: 'rename', path: oldPath };
      if (async) {
        await assert.rejects(fs.promises.rename(oldPath, newPath), error);
      } else {
        assert.throws(() => fs.renameSync(oldPath, newPath), error);
      }

      assert.deepStrictEqual(fs.readdirSync('/'), ['a', 'alias', 'source']);
      assert.deepStrictEqual(fs.readdirSync('/a'), ['b', 'keep.txt']);
      assert.deepStrictEqual(fs.readdirSync('/a/b'), []);
      assert.strictEqual(fs.readFileSync('/a/keep.txt', 'utf8'), 'keep');
      assert.strictEqual(fs.statSync('/a/keep.txt').nlink, 1);
      assert.strictEqual(fs.readlinkSync('/alias'), target);
      assert.strictEqual(fs.statSync('/alias').isDirectory(), true);
    }

    // Resolving the source's parent must still allow a rename onto itself.
    {
      const fs = create();
      fs.mkdirSync('/a');
      fs.writeFileSync('/a/keep.txt', 'keep');
      fs.symlinkSync('/', '/source');
      if (async) {
        await fs.promises.rename('/source/a', '/a');
      } else {
        fs.renameSync('/source/a', '/a');
      }
      assert.strictEqual(fs.readFileSync('/a/keep.txt', 'utf8'), 'keep');
    }

    // A shared name prefix does not make a directory a descendant.
    {
      const fs = create();
      fs.mkdirSync('/a');
      fs.mkdirSync('/ab');
      fs.symlinkSync('/ab', '/alias');
      if (async) {
        await fs.promises.rename('/a', '/alias/moved');
      } else {
        fs.renameSync('/a', '/alias/moved');
      }
      assert.strictEqual(fs.existsSync('/a'), false);
      assert.strictEqual(fs.statSync('/ab/moved').isDirectory(), true);
    }

    // A path beneath the source can resolve outside it through a symlink.
    {
      const fs = create();
      fs.mkdirSync('/a');
      fs.mkdirSync('/outside');
      fs.writeFileSync('/a/keep.txt', 'keep');
      fs.symlinkSync('/outside', '/a/link');
      if (async) {
        await fs.promises.rename('/a', '/a/link/moved');
      } else {
        fs.renameSync('/a', '/a/link/moved');
      }
      assert.strictEqual(fs.existsSync('/a'), false);
      assert.strictEqual(fs.readFileSync('/outside/moved/keep.txt', 'utf8'), 'keep');
    }

    // Renaming a symlink moves the link itself, without following its target.
    {
      const fs = create();
      fs.mkdirSync('/a/b', { recursive: true });
      fs.symlinkSync('/a', '/alias');
      if (async) {
        await fs.promises.rename('/alias', '/a/b/link');
      } else {
        fs.renameSync('/alias', '/a/b/link');
      }
      assert.strictEqual(fs.existsSync('/alias'), false);
      assert.strictEqual(fs.readlinkSync('/a/b/link'), '/a');
    }
  }
}

test().then(common.mustCall());
