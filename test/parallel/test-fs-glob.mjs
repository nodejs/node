// Flags: --expose-internals
import * as common from '../common/index.mjs';
import tmpdir from '../common/tmpdir.js';
import { resolve, dirname, sep } from 'node:path';
import { mkdir, writeFile, symlink } from 'node:fs/promises';
import { test } from 'node:test';
import assert from 'node:assert';
import glob from 'internal/fs/glob';

tmpdir.refresh();

const fixtureDir = tmpdir.resolve('fixtures');
const absDir = tmpdir.resolve('abs');

async function setup() {
  await mkdir(fixtureDir, { recursive: true });
  await mkdir(absDir, { recursive: true });
  const files = [
    'a/.abcdef/x/y/z/a',
    'a/abcdef/g/h',
    'a/abcfed/g/h',
    'a/b/c/d',
    'a/bc/e/f',
    'a/c/d/c/b',
    'a/cb/e/f',
    'a/x/.y/b',
    'a/z/.y/b',
  ].map((f) => resolve(fixtureDir, f));

  const symlinkTo = resolve(fixtureDir, 'a/symlink/a/b/c');
  const symlinkFrom = '../..';

  for (const file of files) {
    const f = resolve(fixtureDir, file);
    const d = dirname(f);
    await mkdir(d, { recursive: true });
    await writeFile(f, 'i like tests');
  }

  if (!common.isWindows) {
    const d = dirname(symlinkTo);
    await mkdir(d, { recursive: true });
    await symlink(symlinkFrom, symlinkTo, 'dir');
  }

  await Promise.all(['foo', 'bar', 'baz', 'asdf', 'quux', 'qwer', 'rewq'].map(async function(w) {
    await mkdir(resolve(absDir, w), { recursive: true });
  }));
}

await setup();

const patterns = {
  'a/c/d/*/b': ['a/c/d/c/b'],
  'a//c//d//*//b': ['a/c/d/c/b'],
  'a/*/d/*/b': ['a/c/d/c/b'],
  'a/*/+(c|g)/./d': ['a/b/c/d'],
  'a/**/[cg]/../[cg]': [
    'a/abcdef/g',
    'a/abcfed/g',
    'a/b/c',
    'a/c',
    'a/c/d/c',
    common.isWindows ? null : 'a/symlink/a/b/c',
  ],
  'a/{b,c,d,e,f}/**/g': [],
  'a/b/**': ['a/b', 'a/b/c', 'a/b/c/d'],
  'a/{b/**,b/c}': ['a/b', 'a/b/c', 'a/b/c/d'],
  './**/g': ['a/abcdef/g', 'a/abcfed/g'],
  'a/abc{fed,def}/g/h': ['a/abcdef/g/h', 'a/abcfed/g/h'],
  'a/abc{fed/g,def}/**/': ['a/abcdef', 'a/abcdef/g', 'a/abcfed/g'],
  'a/abc{fed/g,def}/**///**/': ['a/abcdef', 'a/abcdef/g', 'a/abcfed/g'],
  '**/a': common.isWindows ? ['a'] : ['a', 'a/symlink/a'],
  '**/a/**': [
    'a',
    'a/abcdef',
    'a/abcdef/g',
    'a/abcdef/g/h',
    'a/abcfed',
    'a/abcfed/g',
    'a/abcfed/g/h',
    'a/b',
    'a/b/c',
    'a/b/c/d',
    'a/bc',
    'a/bc/e',
    'a/bc/e/f',
    'a/c',
    'a/c/d',
    'a/c/d/c',
    'a/c/d/c/b',
    'a/cb',
    'a/cb/e',
    'a/cb/e/f',
    ...(common.isWindows ? [] : [
      'a/symlink',
      'a/symlink/a',
      'a/symlink/a/b',
      'a/symlink/a/b/c',
    ]),
    'a/x',
    'a/z',
  ],
  './**/a': common.isWindows ? ['a'] : ['a', 'a/symlink/a', 'a/symlink/a/b/c/a'],
  './**/a/**/': [
    'a',
    'a/abcdef',
    'a/abcdef/g',
    'a/abcfed',
    'a/abcfed/g',
    'a/b',
    'a/b/c',
    'a/bc',
    'a/bc/e',
    'a/c',
    'a/c/d',
    'a/c/d/c',
    'a/cb',
    'a/cb/e',
    ...(common.isWindows ? [] : [
      'a/symlink',
      'a/symlink/a',
      'a/symlink/a/b',
      'a/symlink/a/b/c',
      'a/symlink/a/b/c/a',
      'a/symlink/a/b/c/a/b',
      'a/symlink/a/b/c/a/b/c',
    ]),
    'a/x',
    'a/z',
  ],
  './**/a/**': [
    'a',
    'a/abcdef',
    'a/abcdef/g',
    'a/abcdef/g/h',
    'a/abcfed',
    'a/abcfed/g',
    'a/abcfed/g/h',
    'a/b',
    'a/b/c',
    'a/b/c/d',
    'a/bc',
    'a/bc/e',
    'a/bc/e/f',
    'a/c',
    'a/c/d',
    'a/c/d/c',
    'a/c/d/c/b',
    'a/cb',
    'a/cb/e',
    'a/cb/e/f',
    ...(common.isWindows ? [] : [
      'a/symlink',
      'a/symlink/a',
      'a/symlink/a/b',
      'a/symlink/a/b/c',
      'a/symlink/a/b/c/a',
      'a/symlink/a/b/c/a/b',
      'a/symlink/a/b/c/a/b/c',
    ]),
    'a/x',
    'a/z',
  ],
  './**/a/**/a/**/': common.isWindows ? [] : [
    'a/symlink/a',
    'a/symlink/a/b',
    'a/symlink/a/b/c',
    'a/symlink/a/b/c/a',
    'a/symlink/a/b/c/a/b',
    'a/symlink/a/b/c/a/b/c',
    'a/symlink/a/b/c/a/b/c/a',
    'a/symlink/a/b/c/a/b/c/a/b',
    'a/symlink/a/b/c/a/b/c/a/b/c',
  ],
  '+(a|b|c)/a{/,bc*}/**': [
    'a/abcdef',
    'a/abcdef/g',
    'a/abcdef/g/h',
    'a/abcfed',
    'a/abcfed/g',
    'a/abcfed/g/h',
  ],
  '*/*/*/f': ['a/bc/e/f', 'a/cb/e/f'],
  './**/f': ['a/bc/e/f', 'a/cb/e/f'],
  'a/symlink/a/b/c/a/b/c/a/b/c//a/b/c////a/b/c/**/b/c/**': common.isWindows ? [] : [
    'a/symlink/a/b/c/a/b/c/a/b/c/a/b/c/a/b/c/a/b/c',
    'a/symlink/a/b/c/a/b/c/a/b/c/a/b/c/a/b/c/a/b/c/a',
    'a/symlink/a/b/c/a/b/c/a/b/c/a/b/c/a/b/c/a/b/c/a/b',
    'a/symlink/a/b/c/a/b/c/a/b/c/a/b/c/a/b/c/a/b/c/a/b/c',
  ],
  [`{./*/*,${absDir}/*}`]: [
    `${absDir}/asdf`,
    `${absDir}/bar`,
    `${absDir}/baz`,
    `${absDir}/foo`,
    `${absDir}/quux`,
    `${absDir}/qwer`,
    `${absDir}/rewq`,
    'a/abcdef',
    'a/abcfed',
    'a/b',
    'a/bc',
    'a/c',
    'a/cb',
    common.isWindows ? null : 'a/symlink',
    'a/x',
    'a/z',
  ],
  [`{${absDir}/*,*}`]: [
    `${absDir}/asdf`,
    `${absDir}/bar`,
    `${absDir}/baz`,
    `${absDir}/foo`,
    `${absDir}/quux`,
    `${absDir}/qwer`,
    `${absDir}/rewq`,
    'a',
  ],
  'a/!(symlink)/**': [
    'a/abcdef',
    'a/abcdef/g',
    'a/abcdef/g/h',
    'a/abcfed',
    'a/abcfed/g',
    'a/abcfed/g/h',
    'a/b',
    'a/b/c',
    'a/b/c/d',
    'a/bc',
    'a/bc/e',
    'a/bc/e/f',
    'a/c',
    'a/c/d',
    'a/c/d/c',
    'a/c/d/c/b',
    'a/cb',
    'a/cb/e',
    'a/cb/e/f',
    'a/x',
    'a/z',
  ],
  'a/symlink/a/**/*': common.isWindows ? [] : [
    'a/symlink/a/b',
    'a/symlink/a/b/c',
    'a/symlink/a/b/c/a',
  ],
  'a/!(symlink)/**/..': [
    'a',
    'a/abcdef',
    'a/abcfed',
    'a/b',
    'a/bc',
    'a/c',
    'a/c/d',
    'a/cb',
  ],
  'a/!(symlink)/**/../': [
    'a',
    'a/abcdef',
    'a/abcfed',
    'a/b',
    'a/bc',
    'a/c',
    'a/c/d',
    'a/cb',
  ],
  'a/!(symlink)/**/../*': [
    'a/abcdef',
    'a/abcdef/g',
    'a/abcfed',
    'a/abcfed/g',
    'a/b',
    'a/b/c',
    'a/bc',
    'a/bc/e',
    'a/c',
    'a/c/d',
    'a/c/d/c',
    'a/cb',
    'a/cb/e',
    common.isWindows ? null : 'a/symlink',
    'a/x',
    'a/z',
  ],
  'a/!(symlink)/**/../*/*': [
    'a/abcdef/g',
    'a/abcdef/g/h',
    'a/abcfed/g',
    'a/abcfed/g/h',
    'a/b/c',
    'a/b/c/d',
    'a/bc/e',
    'a/bc/e/f',
    'a/c/d',
    'a/c/d/c',
    'a/c/d/c/b',
    'a/cb/e',
    'a/cb/e/f',
    common.isWindows ? null : 'a/symlink/a',
  ],
};

for (const [pattern, expected] of Object.entries(patterns)) {
  test(pattern, () => {
    const actual = new glob.Glob([pattern], { cwd: fixtureDir }).globSync().sort();
    const normalized = expected.filter(Boolean).map((item) => item.replaceAll('/', sep)).sort();
    assert.deepStrictEqual(actual, normalized);
  });
}
