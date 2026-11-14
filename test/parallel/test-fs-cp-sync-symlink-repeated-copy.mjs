// Repeated copies may replace identical links to unrelated directories or
// children of dest, but must still reject links to dest or its ancestors.
// Refs: https://github.com/nodejs/node/issues/65097
import { mustNotMutateObjectDeep } from '../common/index.mjs';
import { nextdir } from '../common/fs.js';
import assert from 'node:assert';
import { cpSync, mkdirSync, readlinkSync, realpathSync, symlinkSync } from 'node:fs';
import { join, relative } from 'node:path';
import tmpdir from '../common/tmpdir.js';

tmpdir.refresh();

// Preserve coverage for initially missing destinations and relative targets.
for (const relativeTarget of [false, true]) {
  const root = nextdir();
  const src = join(root, 'src');
  const dest = join(root, 'dest');
  const target = join(root, 'target');
  mkdirSync(src, { recursive: true });
  mkdirSync(target);
  symlinkSync(relativeTarget ? '../target' : target, join(src, 'link'), 'dir');
  const opts = mustNotMutateObjectDeep({ recursive: true });
  cpSync(src, dest, opts);
  cpSync(src, dest, opts);
  assert.strictEqual(realpathSync(join(dest, 'link')), realpathSync(target));
}

for (const filtered of [false, true]) {
  for (const aliased of [false, true]) {
    for (const relativeTarget of [false, true]) {
      for (const location of ['outside', 'inside', 'self', 'ancestor']) {
        const root = nextdir();
        const src = join(root, 'src');
        const actual = join(root, 'actual');
        mkdirSync(src, { recursive: true });
        mkdirSync(join(actual, 'dest'), { recursive: true });
        let dest = join(actual, 'dest');
        if (aliased) {
          const alias = join(root, 'alias');
          symlinkSync(actual, alias, 'junction');
          dest = join(alias, 'dest');
        }

        const target = {
          outside: join(root, 'target'),
          inside: join(actual, 'dest', 'sub'),
          self: join(actual, 'dest'),
          ancestor: actual,
        }[location];
        mkdirSync(target, { recursive: true });
        const canonicalTarget = realpathSync(target);
        symlinkSync(relativeTarget ? relative(src, canonicalTarget) : canonicalTarget,
                    join(src, 'link'), 'dir');
        const opts = mustNotMutateObjectDeep({
          recursive: true,
          ...(filtered ? { filter: () => true } : {}),
        });
        cpSync(src, dest, opts);
        const link = join(dest, 'link');
        const originalTarget = readlinkSync(link);
        if (location === 'self' || location === 'ancestor') {
          assert.throws(() => cpSync(src, dest, opts), { code: 'ERR_FS_CP_EINVAL' });
          assert.strictEqual(readlinkSync(link), originalTarget);
        } else {
          cpSync(src, dest, opts);
        }
        assert.strictEqual(realpathSync(link), canonicalTarget);
      }
    }
  }
}
