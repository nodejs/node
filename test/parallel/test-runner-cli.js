'use strict';
require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const { join } = require('path');
const fixtures = require('../common/fixtures');
const testFixtures = fixtures.path('test-runner');

{
  // File not found.
  const args = ['--test', 'a-random-file-that-does-not-exist.js'];
  const child = spawnSync(process.execPath, args);

  assert.strictEqual(child.status, 1);
  assert.strictEqual(child.signal, null);
  assert.strictEqual(child.stdout.toString(), '');
  assert(/^Could not find/.test(child.stderr.toString()));
}

{
  // Default behavior. node_modules is ignored. Files that don't match the
  // pattern are ignored except in test/ directories.
  const args = ['--test', testFixtures];
  const child = spawnSync(process.execPath, args);

  assert.strictEqual(child.status, 1);
  assert.strictEqual(child.signal, null);
  assert.strictEqual(child.stderr.toString(), '');
  const stdout = child.stdout.toString();
  assert(/ok 1 - .+index\.test\.js/.test(stdout));
  assert(/not ok 2 - .+random\.test\.mjs/.test(stdout));
  assert(/not ok 1 - this should fail/.test(stdout));
  assert(/ok 3 - .+subdir.+subdir_test\.js/.test(stdout));
  assert(/ok 4 - .+random\.cjs/.test(stdout));
}

{
  // User specified files that don't match the pattern are still run.
  const args = ['--test', testFixtures, join(testFixtures, 'index.js')];
  const child = spawnSync(process.execPath, args);

  assert.strictEqual(child.status, 1);
  assert.strictEqual(child.signal, null);
  assert.strictEqual(child.stderr.toString(), '');
  const stdout = child.stdout.toString();
  assert(/not ok 1 - .+index\.js/.test(stdout));
  assert(/ok 2 - .+index\.test\.js/.test(stdout));
  assert(/not ok 3 - .+random\.test\.mjs/.test(stdout));
  assert(/not ok 1 - this should fail/.test(stdout));
  assert(/ok 4 - .+subdir.+subdir_test\.js/.test(stdout));
}

{
  // Searches node_modules if specified.
  const args = ['--test', join(testFixtures, 'node_modules')];
  const child = spawnSync(process.execPath, args);

  assert.strictEqual(child.status, 1);
  assert.strictEqual(child.signal, null);
  assert.strictEqual(child.stderr.toString(), '');
  const stdout = child.stdout.toString();
  assert(/not ok 1 - .+test-nm\.js/.test(stdout));
}

{
  // The current directory is used by default.
  const args = ['--test'];
  const options = { cwd: testFixtures };
  const child = spawnSync(process.execPath, args, options);

  assert.strictEqual(child.status, 1);
  assert.strictEqual(child.signal, null);
  assert.strictEqual(child.stderr.toString(), '');
  const stdout = child.stdout.toString();
  assert(/ok 1 - .+index\.test\.js/.test(stdout));
  assert(/not ok 2 - .+random\.test\.mjs/.test(stdout));
  assert(/not ok 1 - this should fail/.test(stdout));
  assert(/ok 3 - .+subdir.+subdir_test\.js/.test(stdout));
  assert(/ok 4 - .+random\.cjs/.test(stdout));
}

{
  // Flags that cannot be combined with --test.
  const flags = [
    ['--check', '--test'],
    ['--interactive', '--test'],
    ['--eval', 'console.log("should not print")', '--test'],
    ['--print', 'console.log("should not print")', '--test'],
  ];

  if (process.features.inspector) {
    flags.push(
      ['--inspect', '--test'],
      ['--inspect-brk', '--test'],
    );
  }

  flags.forEach((args) => {
    const child = spawnSync(process.execPath, args);

    assert.notStrictEqual(child.status, 0);
    assert.strictEqual(child.signal, null);
    assert.strictEqual(child.stdout.toString(), '');
    const stderr = child.stderr.toString();
    assert(/--test/.test(stderr));
  });
}
