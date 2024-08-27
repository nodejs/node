'use strict';

const common = require('../common');
const { test } = require('node:test');
const fixtures = require('../common/fixtures');
const fixture = fixtures.path('test-runner/includes-excludes/');

async function spawnTestProcess(globs, arg) {
  const args = ['--test', '--test-reporter=spec'];
  for (const glob of globs) {
    args.push(`${arg}=${glob}`);
  }

  return common.spawnPromisified(process.execPath, args, { cwd: fixture });
}

async function buildTests(tests, flag) {
  test(flag, async (t) => {
    for (const { globs, skip, includes } of tests) {
      await t.test(globs.join(', ') || 'No globs', async (subtest) => {
        const child = await spawnTestProcess(globs, flag);
        for (const skipped of skip) {
          subtest.assert.ok(child.stdout.includes(`﹣ ${skipped}`));
        }
        for (const included of includes) {
          subtest.assert.ok(!child.stdout.includes(`﹣ ${included}`));
          subtest.assert.ok(child.stdout.includes(`✔ ${included}`));
        }
        subtest.assert.strictEqual(child.code, 0);
      });
    }
  });
}

buildTests([
  { globs: [], skip: [], includes: ['Index', 'Other', 'Inner'] },
  { globs: ['*.test.js'], skip: ['subdir/inner.test.js'], includes: ['Index', 'Other'] },
  { globs: ['nonexistent.js'], skip: ['subdir/inner.test.js', 'other.test.js', 'index.test.js'], includes: [] },
], '--test-runner-include');

buildTests([
  { globs: [], skip: [], includes: ['Index', 'Other', 'Inner'] },
  { globs: ['o*.test.js', 'i*.test.js'], skip: ['other.test.js', 'index.test.js'], includes: ['Inner'] },
  { globs: ['**'], skip: ['subdir/inner.test.js', 'other.test.js', 'index.test.js'], includes: [] },
], '--test-runner-exclude');
