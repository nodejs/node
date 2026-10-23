'use strict';
const common = require('../common');
const assert = require('node:assert');
const { writeFileSync } = require('node:fs');
const { test, run } = require('node:test');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();
const fixture = tmpdir.resolve('only.test.js');
writeFileSync(fixture, `
  const { test } = require('node:test');
  test.only('focused test', () => {});
  test('ordinary test', () => {});
`);
const env = { ...process.env, NODE_OPTIONS: '', FORCE_COLOR: '0' };
delete env.NODE_TEST_CONTEXT;
const onlyIfNodeOptionsSupport = { skip: process.config.variables.node_without_node_options };

function createFixture(name, source) {
  const file = tmpdir.resolve(`${name}.js`);
  writeFileSync(file, `
    const { test, it, suite, describe } = require('node:test');
    ${source}
  `);
  return file;
}

function assertForbidden({ code, signal, stdout, stderr }) {
  assert.strictEqual(signal, null);
  assert.strictEqual(code, 1, stdout + stderr);
  assert.match(stdout + stderr, /(?:only|runOnly).*not allowed/);
  assert.doesNotMatch(stdout, /BODY_EXECUTED/);
}

const cases = {
  'test-only': "test.only('focused test', () => { console.log('BODY_EXECUTED'); });",
  'it-only': "it.only('focused test', () => { console.log('BODY_EXECUTED'); });",
  'suite-only': "suite.only('focused suite', () => { console.log('BODY_EXECUTED'); });",
  'describe-only': "describe.only('focused suite', () => { console.log('BODY_EXECUTED'); });",
  'only-option': "test('focused test', { only: true }, () => { console.log('BODY_EXECUTED'); });",
  'suite-option': "suite('focused suite', { only: true }, () => { console.log('BODY_EXECUTED'); });",
  'subtest-option': "test('parent', async (t) => { await t.test('child', { only: true }, () => {}); });",
  'run-only': "test('parent', (t) => { t.runOnly(true); console.log('BODY_EXECUTED'); });",
  'skipped-only': "test('focused test', { only: true, skip: true }, () => {});",
  'todo-only': "test('focused test', { only: true, todo: true }, () => {});",
  'expected-failure-only': "test('focused test', { only: true, expectFailure: true }, () => {});",
  'todo-parent': "test.todo('parent', () => { test.only('child', () => {}); });",
  'expected-failure-parent': "test('parent', { expectFailure: true }, (t) => { t.runOnly(true); });",
};
const caseFiles = Object.entries(cases).map(([name, source]) => [name, createFixture(name, source)]);

for (const isolation of ['process', 'none']) {
  test(`run() rejects only with isolation=${isolation}`, async () => {
    const runner = tmpdir.resolve(`runner-${isolation}.js`);
    writeFileSync(runner, `
        const { run } = require('node:test');
        const { tap } = require('node:test/reporters');
        run({
          files: [${JSON.stringify(fixture)}],
          isolation: ${JSON.stringify(isolation)},
          forbidOnly: true,
        }).on('test:fail', () => { process.exitCode = 1; }).compose(tap).pipe(process.stdout);
      `);
    const { code, signal, stdout, stderr } = await common.spawnPromisified(process.execPath, [runner], { env });
    assertForbidden({ code, signal, stdout, stderr });
  });

  for (const [name, file] of caseFiles) {
    test(`CLI rejects ${name} with isolation=${isolation}`, async () => {
      assertForbidden(await common.spawnPromisified(process.execPath, [
        '--test', `--test-isolation=${isolation}`, '--test-reporter=tap', '--test-forbid-only', file,
      ], { env }));
    });
  }

  for (const args of [['--test-only'], ['--test-name-pattern=ordinary'], ['--test-skip-pattern=focused']]) {
    test(`filter ${args[0]} cannot hide only with isolation=${isolation}`, async () => {
      assertForbidden(await common.spawnPromisified(process.execPath, [
        '--test', `--test-isolation=${isolation}`, '--test-reporter=tap', '--test-forbid-only', ...args, fixture,
      ], { env }));
    });
  }

  test(`NODE_OPTIONS enables forbidOnly with isolation=${isolation}`, onlyIfNodeOptionsSupport, async () => {
    assertForbidden(await common.spawnPromisified(process.execPath, [
      '--test', `--test-isolation=${isolation}`, '--test-reporter=tap', fixture,
    ], { env: { ...env, NODE_OPTIONS: '--test-forbid-only' } }));
  });

  test(`filtered suites cannot hide only with isolation=${isolation}`, async () => {
    const file = createFixture(`filtered-${isolation}`, `
      suite('filtered', () => { it.only('child', () => {}); });
      test('ordinary', () => {});
    `);
    assertForbidden(await common.spawnPromisified(process.execPath, [
      '--test', `--test-isolation=${isolation}`, '--test-reporter=tap',
      '--test-forbid-only', '--test-name-pattern=ordinary', file,
    ], { env }));
  });

  for (const source of ['CLI', 'NODE_OPTIONS']) {
    const options = source === 'NODE_OPTIONS' ? onlyIfNodeOptionsSupport : {};
    test(`run() can override ${source} with forbidOnly=false and isolation=${isolation}`, options, async () => {
      const runner = createFixture(`override-${source}-${isolation}`, `
        const { run } = require('node:test');
        const { tap } = require('node:test/reporters');
        run({ files: [${JSON.stringify(fixture)}], isolation: ${JSON.stringify(isolation)}, forbidOnly: false })
          .on('test:fail', () => { process.exitCode = 1; }).compose(tap).pipe(process.stdout);
      `);
      const args = source === 'CLI' ? ['--test-forbid-only', runner] : [runner];
      const childEnv = source === 'NODE_OPTIONS' ? { ...env, NODE_OPTIONS: '--test-forbid-only' } : env;
      const { code, signal, stdout, stderr } = await common.spawnPromisified(process.execPath, args, { env: childEnv });
      assert.strictEqual(signal, null);
      assert.strictEqual(code, 0, stdout + stderr);
      assert.match(stdout, /ok 1 - focused test/);
    });
  }

  test(`ordinary tests still run with isolation=${isolation}`, async () => {
    const file = createFixture(`ordinary-${isolation}`, `
      test('ordinary', () => {});
      test('explicit false', { only: false }, () => {});
      test('runOnly false', (t) => { t.runOnly(false); });
    `);
    const { code, signal, stdout, stderr } = await common.spawnPromisified(process.execPath, [
      '--test', `--test-isolation=${isolation}`, '--test-reporter=tap', '--test-forbid-only', file,
    ], { env });
    assert.strictEqual(signal, null);
    assert.strictEqual(code, 0, stdout + stderr);
    // Explicit only:false is filtered out in the existing none-isolation mode.
    assert.match(stdout, isolation === 'none' ? /# pass 2/ : /# pass 3/);
  });

  for (const flags of [[], ['--test-forbid-only', '--no-test-forbid-only']]) {
    test(`only still filters when disabled (${flags}) with isolation=${isolation}`, async () => {
      const { code, signal, stdout, stderr } = await common.spawnPromisified(process.execPath, [
        '--test', `--test-isolation=${isolation}`, '--test-reporter=tap', '--test-only', ...flags, fixture,
      ], { env });
      assert.strictEqual(signal, null);
      assert.strictEqual(code, 0, stdout + stderr);
      assert.match(stdout, /# pass 1/);
      assert.doesNotMatch(stdout, /ordinary test/);
    });
  }
}

test('run() validates forbidOnly', () => {
  for (const forbidOnly of [0, 1, '', 'true', {}, []]) {
    assert.throws(() => run({ forbidOnly }), {
      code: 'ERR_INVALID_ARG_TYPE',
    });
  }
});
