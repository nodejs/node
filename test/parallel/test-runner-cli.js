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
  assert.match(child.stderr.toString(), /^Could not find/);
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
  assert.match(stdout, /ok 1 - .+index\.test\.js/);
  assert.match(stdout, /not ok 2 - .+random\.test\.mjs/);
  assert.match(stdout, /not ok 1 - this should fail/);
  assert.match(stdout, /ok 3 - .+subdir.+subdir_test\.js/);
  assert.match(stdout, /ok 4 - .+random\.cjs/);
}

{
  // Same but with a prototype mutation in require scripts.
  const args = ['--require', join(testFixtures, 'protoMutation.js'), '--test', testFixtures];
  const child = spawnSync(process.execPath, args);

  const stdout = child.stdout.toString();
  assert.match(stdout, /ok 1 - .+index\.test\.js/);
  assert.match(stdout, /not ok 2 - .+random\.test\.mjs/);
  assert.match(stdout, /not ok 1 - this should fail/);
  assert.match(stdout, /ok 3 - .+subdir.+subdir_test\.js/);
  assert.match(stdout, /ok 4 - .+random\.cjs/);
  assert.strictEqual(child.status, 1);
  assert.strictEqual(child.signal, null);
  assert.strictEqual(child.stderr.toString(), '');
}

{
  // User specified files that don't match the pattern are still run.
  const args = ['--test', testFixtures, join(testFixtures, 'index.js')];
  const child = spawnSync(process.execPath, args);

  assert.strictEqual(child.status, 1);
  assert.strictEqual(child.signal, null);
  assert.strictEqual(child.stderr.toString(), '');
  const stdout = child.stdout.toString();
  assert.match(stdout, /not ok 1 - .+index\.js/);
  assert.match(stdout, /ok 2 - .+index\.test\.js/);
  assert.match(stdout, /not ok 3 - .+random\.test\.mjs/);
  assert.match(stdout, /not ok 1 - this should fail/);
  assert.match(stdout, /ok 4 - .+subdir.+subdir_test\.js/);
}

{
  // Searches node_modules if specified.
  const args = ['--test', join(testFixtures, 'node_modules')];
  const child = spawnSync(process.execPath, args);

  assert.strictEqual(child.status, 1);
  assert.strictEqual(child.signal, null);
  assert.strictEqual(child.stderr.toString(), '');
  const stdout = child.stdout.toString();
  assert.match(stdout, /not ok 1 - .+test-nm\.js/);
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
  assert.match(stdout, /ok 1 - .+index\.test\.js/);
  assert.match(stdout, /not ok 2 - .+random\.test\.mjs/);
  assert.match(stdout, /not ok 1 - this should fail/);
  assert.match(stdout, /ok 3 - .+subdir.+subdir_test\.js/);
  assert.match(stdout, /ok 4 - .+random\.cjs/);
}

{
  // Flags that cannot be combined with --test.
  const flags = [
    ['--check', '--test'],
    ['--interactive', '--test'],
    ['--eval', 'console.log("should not print")', '--test'],
    ['--print', 'console.log("should not print")', '--test'],
  ];


  flags.forEach((args) => {
    const child = spawnSync(process.execPath, args);

    assert.notStrictEqual(child.status, 0);
    assert.strictEqual(child.signal, null);
    assert.strictEqual(child.stdout.toString(), '');
    const stderr = child.stderr.toString();
    assert.match(stderr, /--test/);
  });
}
