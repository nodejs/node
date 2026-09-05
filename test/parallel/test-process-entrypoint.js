'use strict';

const { spawnPromisified } = require('../common');
const fixtures = require('../common/fixtures');

const assert = require('node:assert');
const { execPath } = require('node:process');
const { describe, it } = require('node:test');

const cjsFixture = fixtures.path('entrypoint', 'commonjs.cjs');
const cjsFixtureURL = fixtures.fileURL('entrypoint', 'commonjs.cjs').href;

const mjsFixture = fixtures.path('entrypoint', 'module.mjs');
const mjsFixtureURL = fixtures.fileURL('entrypoint', 'module.mjs').href;

const workerFixture = fixtures.path('entrypoint', 'worker.cjs');
const workerFixtureURL = fixtures.fileURL('entrypoint', 'worker.cjs').href;

const cjsPreload = fixtures.path('entrypoint', 'check-commonjs.cjs');
const esmPreload = fixtures.fileURL('entrypoint', 'check-module.mjs').href;
const loader = fixtures.fileURL('entrypoint', 'loader.mjs').href;

const printEntrypoint = 'console.log(process.entrypoint)';

async function getEntrypoint(args, options, expected) {
  const { code, signal, stderr, stdout } = await spawnPromisified(
    execPath,
    args,
    {
      ...options,
      env: {
        ...process.env,
        NODE_TEST_ENTRYPOINT: expected,
      },
    },
  );
  assert.strictEqual(code, 0, stderr);
  assert.strictEqual(signal, null);

  return stdout.trim();
}

const testCases = [
  {
    name: 'is the file URL of a CommonJS entry point',
    args: [cjsFixture],
    expected: cjsFixtureURL,
  },
  {
    name: 'is the file URL of an ES module entry point',
    args: [mjsFixture],
    expected: mjsFixtureURL,
  },
  {
    name: 'is absolute when passed a relative path',
    args: ['./commonjs.cjs'],
    options: { cwd: fixtures.path('entrypoint') },
    expected: cjsFixtureURL,
  },
  {
    name: 'stays correct when using --require',
    args: ['--require', cjsPreload, cjsFixture],
    expected: cjsFixtureURL,
  },
  {
    name: 'stays correct when using --import',
    args: ['--import', esmPreload, cjsFixture],
    expected: cjsFixtureURL,
  },
  {
    name: 'stays correct when using --loader',
    args: ['--loader', loader, cjsFixture],
    expected: cjsFixtureURL,
  },
  {
    name: 'stays correct when combining preloads and loaders',
    args: [
      '--require', cjsPreload,
      '--import', esmPreload,
      '--loader', loader,
      mjsFixture,
    ],
    expected: mjsFixtureURL,
  },
  {
    name: 'is inherited by worker threads and their preloads and loaders',
    args: [
      '--require', cjsPreload,
      '--import', esmPreload,
      '--loader', loader,
      workerFixture,
    ],
    expected: workerFixtureURL,
  },
  {
    name: 'supports non-file --entry-url entry points',
    args: ['--entry-url', `data:text/javascript,${printEntrypoint}`],
    expected: `data:text/javascript,${printEntrypoint}`,
  },
  {
    name: 'is undefined when evaluating CommonJS without an entry point',
    args: ['--eval', printEntrypoint],
    expected: 'undefined',
  },
  {
    name: 'is undefined when evaluating an ES module without an entry point',
    args: ['--input-type=module', '--eval', printEntrypoint],
    expected: 'undefined',
  },
];

describe('process.entrypoint', { concurrency: true }, () => {
  for (const { name, args, options, expected } of testCases) {
    it(name, async () => {
      assert.strictEqual(await getEntrypoint(args, options, expected), expected);
    });
  }
});
