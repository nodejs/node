'use strict';
// Flags: --expose-internals
/* eslint-disable no-template-curly-in-string */

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const fs = require('node:fs');
const path = require('node:path');
const { test, describe } = require('node:test');

const fixtureSources = {
  'inline/basic.js': [
    "'use strict';",
    "const { test } = require('node:test');",
    '',
    '// One object + one primitive to create two snapshots.',
    '// Both omit expected snapshot literal initially.',
    '',
    "test('inline basic', (t) => {",
    '  t.assert.inlineSnapshot({ foo: 1, bar: 2 });',
    "  t.assert.inlineSnapshot('hello world');",
    '});',
    '',
  ].join('\n'),
  'inline/replace.js': [
    "'use strict';",
    "const { test } = require('node:test');",
    '',
    'let value = 1;',
    '',
    "test('inline replace', (t) => {",
    '  t.assert.inlineSnapshot({ value });',
    '});',
    '',
  ].join('\n'),
  'inline/escape.js': [
    "'use strict';",
    "const { test } = require('node:test');",
    '',
    "const tricky = 'b`ack\\slash ${interp} end';",
    '',
    "test('inline escape', (t) => {",
    '  t.assert.inlineSnapshot(tricky);',
    '});',
    '',
  ].join('\n'),
  'inline/explicit.js': [
    "'use strict';",
    "const { test } = require('node:test');",
    '',
    "test('inline explicit', (t) => {",
    '  t.assert.inlineSnapshot({ a: 1 }, `',
    '{',
    '  "a": 2',
    '}',
    '`); // incorrect on purpose',
    '});',
    '',
  ].join('\n'),
  'inline/missing.js': [
    "'use strict';",
    "const { test } = require('node:test');",
    '',
    "test('inline missing', (t) => {",
    "  t.assert.inlineSnapshot('abc');",
    '});',
    '',
  ].join('\n'),
  'inline/template-with-expressions.js': [
    "'use strict';",
    "const { test } = require('node:test');",
    '',
    "test('inline template with expressions', (t) => {",
    '  const value = "test";',
    '  t.assert.inlineSnapshot(value, `template with ${value} expression`);',
    '});',
    '',
  ].join('\n'),
  'inline/template-literal.js': [
    "'use strict';",
    "const { test } = require('node:test');",
    '',
    "test('inline template literal', (t) => {",
    '  t.assert.inlineSnapshot("hello world", `',
    'hello world',
    '`);',
    '});',
    '',
  ].join('\n'),
  'inline/typescript.ts': [
    "import { test } from 'node:test';",
    '',
    'interface User {',
    '  name: string;',
    '  age: number;',
    '}',
    '',
    "test('typescript inline snapshot', (t) => {",
    '  const user: User = { name: "Alice", age: 30 };',
    '  t.assert.inlineSnapshot(user);',
    "  t.assert.inlineSnapshot('TypeScript works!');",
    '});',
    '',
  ].join('\n'),
  'inline/commonjs.cjs': [
    "'use strict';",
    "const { test } = require('node:test');",
    '',
    "test('commonjs inline snapshot', (t) => {",
    '  const data = { module: "CommonJS", working: true };',
    '  t.assert.inlineSnapshot(data);',
    "  t.assert.inlineSnapshot('CJS format works!');",
    '});',
    '',
  ].join('\n'),
  'inline/esmodule.mjs': [
    "import { test } from 'node:test';",
    '',
    "test('esmodule inline snapshot', (t) => {",
    '  const data = { module: "ES Module", working: true };',
    '  t.assert.inlineSnapshot(data);',
    "  t.assert.inlineSnapshot('MJS format works!');",
    '});',
    '',
  ].join('\n'),
  'inline/mixed-syntax.js': [
    "'use strict';",
    '// This file has mixed syntax that might confuse the parser',
    "const { test } = require('node:test');",
    '',
    '// This comment mentions import but file uses require()',
    "// import { something } from 'somewhere'; // commented out",
    '',
    "test('mixed syntax parsing', (t) => {",
    '  const data = { parser: "should fallback to script mode", success: true };',
    '  t.assert.inlineSnapshot(data);',
    "  t.assert.inlineSnapshot('Mixed syntax handled correctly');",
    '});',
    '',
  ].join('\n'),
  'inline/direct-call.js': [
    "'use strict';",
    "const { test } = require('node:test');",
    '',
    "test('direct inlineSnapshot call pattern', (t) => {",
    '  // Destructure inlineSnapshot from t.assert for direct usage',
    '  const { inlineSnapshot } = t.assert;',
    '  const result = { method: "direct", pattern: "inlineSnapshot(...)" };',
    '  // Test direct call pattern without member expression',
    '  inlineSnapshot(result);',
    "  inlineSnapshot('Direct call pattern test');",
    '});',
    '',
  ].join('\n'),
  'inline/renamed-call.js': [
    "'use strict';",
    "const { test } = require('node:test');",
    '',
    "test('renamed inlineSnapshot call pattern', (t) => {",
    '  // Destructure and rename inlineSnapshot for direct usage',
    '  const { inlineSnapshot: snap } = t.assert;',
    '  const result = { method: "renamed", pattern: "snap(...)" };',
    '  // Test renamed direct call pattern - should NOT be detected',
    '  snap(result);',
    "  snap('Renamed call pattern test');",
    '  // But regular member expression should still work',
    '  t.assert.inlineSnapshot({ fallback: "member expression" });',
    '});',
    '',
  ].join('\n'),
  'inline/custom-serializers.js': [
    "'use strict';",
    "const { test, snapshot } = require('node:test');",
    '',
    '// Set custom serializers',
    'snapshot.setDefaultSnapshotSerializers([',
    '  (value) => JSON.stringify(value).toUpperCase(),',
    '  (value) => `CUSTOM: ${value}`',
    ']);',
    '',
    "test('inline snapshot with custom serializers for complex objects', (t) => {",
    '  const complexData = { test: "custom serializer" };',
    '  // Complex objects should use custom serializers',
    '  t.assert.inlineSnapshot(complexData);',
    '});',
    '',
  ].join('\n'),
  'inline/primitives-serializers.js': [
    "'use strict';",
    "const { test, snapshot } = require('node:test');",
    '',
    '// Set custom serializers',
    'snapshot.setDefaultSnapshotSerializers([',
    '  (value) => JSON.stringify(value).toUpperCase(),',
    '  (value) => `CUSTOM: ${value}`',
    ']);',
    '',
    "test('inline snapshot with primitives ignores custom serializers', (t) => {",
    '  const stringData = "just a string";',
    '  const numberData = 42;',
    '  const booleanData = true;',
    '  const nullData = null;',
    '  const undefinedData = undefined;',
    '  // Primitives should NOT use custom serializers',
    '  t.assert.inlineSnapshot(stringData);',
    '  t.assert.inlineSnapshot(numberData);',
    '  t.assert.inlineSnapshot(booleanData);',
    '  t.assert.inlineSnapshot(nullData);',
    '  t.assert.inlineSnapshot(undefinedData);',
    '});',
    '',
  ].join('\n')
};

function writeTestFile(rel, contents) {
  const file = tmpdir.resolve(rel);
  fs.mkdirSync(path.dirname(file), { recursive: true });
  fs.writeFileSync(file, contents);
  return file;
}

// Helper to assert basic pass/fail summary lines.
function assertSummary(t, stdout, { tests, pass, fail }) {
  t.assert.match(stdout, new RegExp(`tests ${tests}`));
  t.assert.match(stdout, new RegExp(`pass ${pass}`));
  t.assert.match(stdout, new RegExp(`fail ${fail}`));
}

describe('inline snapshot tests', () => {
  test('insertion workflow', async (t) => {
    tmpdir.refresh();
    const testFile = writeTestFile('inline/basic.js', fixtureSources['inline/basic.js']);

    // Run without update flag -> should fail, missing inline snapshots.
    const first = await common.spawnPromisified(
      process.execPath,
      ['--test', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(first.code, 1);
    t.assert.match(first.stdout, /Missing inline snapshots/i);
    assertSummary(t, first.stdout, { tests: 1, pass: 0, fail: 1 });

    // Run with update flag -> generates inline snapshots and passes.
    const second = await common.spawnPromisified(
      process.execPath,
      ['--test', '--test-update-snapshots', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(second.code, 0);
    assertSummary(t, second.stdout, { tests: 1, pass: 1, fail: 0 });

    // Verify file was mutated with inline template literals (backticks present).
    const updatedSource = fs.readFileSync(testFile, 'utf8');
    t.assert.match(updatedSource, /inlineSnapshot\(\{ foo: 1, bar: 2 \}, `\n[\s\S]*\n`\)/);
    t.assert.match(updatedSource, /inlineSnapshot\('hello world', `\nhello world\n`\)/);

    // Run again without update flag -> should still pass.
    const third = await common.spawnPromisified(
      process.execPath,
      ['--test', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(third.code, 0);
    assertSummary(t, third.stdout, { tests: 1, pass: 1, fail: 0 });
  });

  test('replacement on update', async (t) => {
    tmpdir.refresh();
    const testFile = writeTestFile('inline/replace.js', fixtureSources['inline/replace.js']);

    // Generate snapshot.
    await common.spawnPromisified(
      process.execPath,
      ['--test-update-snapshots', '--test', testFile],
      { cwd: tmpdir.path },
    );

    // Modify file (change value initialization) so actual differs.
    let src = fs.readFileSync(testFile, 'utf8');
    src = src.replace('let value = 1;', 'let value = 2;');
    fs.writeFileSync(testFile, src);

    // Run without update flag -> mismatch failure.
    const mismatch = await common.spawnPromisified(
      process.execPath,
      ['--test', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(mismatch.code, 1);
    t.assert.match(mismatch.stdout, /Inline snapshot mismatch|snapshot/i);

    // Run with update flag -> passes and snapshot updated.
    const updated = await common.spawnPromisified(
      process.execPath,
      ['--test-update-snapshots', '--test', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(updated.code, 0);
  });

  test('escaping of special characters', async (t) => {
    tmpdir.refresh();
    const testFile = writeTestFile('inline/escape.js', fixtureSources['inline/escape.js']);

    await common.spawnPromisified(
      process.execPath,
      ['--test-update-snapshots', '--test', testFile],
      { cwd: tmpdir.path },
    );
    const source = fs.readFileSync(testFile, 'utf8');

    t.assert.ok(source.includes('b\\`ackslash \\${interp} end'));
  });

  test('explicit expected literal mismatch', async (t) => {
    tmpdir.refresh();
    const testFile = writeTestFile('inline/explicit.js', fixtureSources['inline/explicit.js']);

    const failRun = await common.spawnPromisified(
      process.execPath,
      ['--test', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(failRun.code, 1);
    t.assert.match(failRun.stdout, /Inline snapshot mismatch|snapshot/i);

    const updateRun = await common.spawnPromisified(
      process.execPath,
      ['--test-update-snapshots', '--test', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(updateRun.code, 0);

    const passRun = await common.spawnPromisified(
      process.execPath,
      ['--test', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(passRun.code, 0);
  });

  test('custom serializers work for complex objects', async (t) => {
    tmpdir.refresh();
    const testFile = writeTestFile('inline/custom-serializers.js', fixtureSources['inline/custom-serializers.js']);

    // Run with update flag to generate inline snapshots
    const updateRun = await common.spawnPromisified(
      process.execPath,
      ['--test', '--test-update-snapshots', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(updateRun.code, 0);

    // Verify the generated snapshots
    const updatedSource = fs.readFileSync(testFile, 'utf8');

    // Complex objects should use custom serializers (uppercased with CUSTOM prefix)
    t.assert.match(updatedSource, /CUSTOM: \{"TEST":"CUSTOM SERIALIZER"\}/);

    // Run again without update flag to ensure it passes
    const passRun = await common.spawnPromisified(
      process.execPath,
      ['--test', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(passRun.code, 0);
    assertSummary(t, passRun.stdout, { tests: 1, pass: 1, fail: 0 });
  });

  test('primitives ignore custom serializers', async (t) => {
    tmpdir.refresh();
    const testFile = writeTestFile(
      'inline/primitives-serializers.js',
      fixtureSources['inline/primitives-serializers.js']
    );

    // Run with update flag to generate inline snapshots
    const updateRun = await common.spawnPromisified(
      process.execPath,
      ['--test', '--test-update-snapshots', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(updateRun.code, 0);

    // Verify the generated snapshots
    const updatedSource = fs.readFileSync(testFile, 'utf8');

    // Primitives should NOT use custom serializers - they should preserve their natural form
    t.assert.match(updatedSource, /inlineSnapshot\(stringData, `\njust a string\n`\)/);
    t.assert.match(updatedSource, /inlineSnapshot\(numberData, `\n42\n`\)/);
    t.assert.match(updatedSource, /inlineSnapshot\(booleanData, `\ntrue\n`\)/);
    t.assert.match(updatedSource, /inlineSnapshot\(nullData, `\nnull\n`\)/);
    t.assert.match(updatedSource, /inlineSnapshot\(undefinedData, `\nundefined\n`\)/);

    // Should NOT contain custom serializer output in the actual snapshots
    t.assert.doesNotMatch(updatedSource, /inlineSnapshot\([^,]+, `[^`]*CUSTOM:/);
    t.assert.doesNotMatch(updatedSource, /`\nCUSTOM:/);
    t.assert.doesNotMatch(updatedSource, /CUSTOM: "JUST A STRING"/);
    t.assert.doesNotMatch(updatedSource, /CUSTOM: 42/);
    t.assert.doesNotMatch(updatedSource, /CUSTOM: TRUE/);
    t.assert.doesNotMatch(updatedSource, /CUSTOM: NULL/);
    t.assert.doesNotMatch(updatedSource, /CUSTOM: UNDEFINED/);

    // Run again without update flag to ensure it passes
    const passRun = await common.spawnPromisified(
      process.execPath,
      ['--test', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(passRun.code, 0);
    assertSummary(t, passRun.stdout, { tests: 1, pass: 1, fail: 0 });
  });

  test('missing snapshot message', async (t) => {
    tmpdir.refresh();
    const testFile = writeTestFile('inline/missing.js', fixtureSources['inline/missing.js']);

    const run = await common.spawnPromisified(
      process.execPath,
      ['--test', testFile],
      { cwd: tmpdir.path },
    );

    t.assert.strictEqual(run.code, 1);
    t.assert.match(run.stdout, /Missing inline snapshots/i);
  });

  test('template literal with expressions should fail', async (t) => {
    tmpdir.refresh();
    const testFile = writeTestFile(
      'inline/template-with-expressions.js',
      fixtureSources['inline/template-with-expressions.js']
    );

    const run = await common.spawnPromisified(
      process.execPath,
      ['--test', testFile],
      { cwd: tmpdir.path },
    );

    t.assert.strictEqual(run.code, 1);
    t.assert.match(run.stdout, /Inline snapshot template must not contain expressions|ERR_INVALID_STATE/i);
  });

  test('template literal without expressions should pass', async (t) => {
    tmpdir.refresh();
    const testFile = writeTestFile('inline/template-literal.js', fixtureSources['inline/template-literal.js']);

    const run = await common.spawnPromisified(
      process.execPath,
      ['--test', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(run.code, 0);
    assertSummary(t, run.stdout, { tests: 1, pass: 1, fail: 0 });
  });

  test('TypeScript file support', async (t) => {
    tmpdir.refresh();
    const testFile = writeTestFile('inline/typescript.ts', fixtureSources['inline/typescript.ts']);

    // Run without update flag -> should fail, missing inline snapshots.
    const first = await common.spawnPromisified(
      process.execPath,
      ['--experimental-strip-types', '--test', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(first.code, 1);
    t.assert.match(first.stdout, /Missing inline snapshots/i);
    assertSummary(t, first.stdout, { tests: 1, pass: 0, fail: 1 });

    // Run with update flag -> generates inline snapshots and passes.
    const second = await common.spawnPromisified(
      process.execPath,
      ['--experimental-strip-types', '--test', '--test-update-snapshots', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(second.code, 0);
    assertSummary(t, second.stdout, { tests: 1, pass: 1, fail: 0 });
  });

  test('CommonJS (.cjs) file support', async (t) => {
    tmpdir.refresh();
    const testFile = writeTestFile('inline/commonjs.cjs', fixtureSources['inline/commonjs.cjs']);

    // Run without update flag -> should fail, missing inline snapshots.
    const first = await common.spawnPromisified(
      process.execPath,
      ['--test', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(first.code, 1);
    t.assert.match(first.stdout, /Missing inline snapshots/i);
    assertSummary(t, first.stdout, { tests: 1, pass: 0, fail: 1 });

    // Run with update flag -> generates inline snapshots and passes.
    const second = await common.spawnPromisified(
      process.execPath,
      ['--test', '--test-update-snapshots', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(second.code, 0);
    assertSummary(t, second.stdout, { tests: 1, pass: 1, fail: 0 });

    // Verify file was mutated with inline template literals.
    const updatedSource = fs.readFileSync(testFile, 'utf8');
    t.assert.match(updatedSource, /inlineSnapshot\(data, `\n[\s\S]*\n`\)/);
    t.assert.match(updatedSource, /inlineSnapshot\('CJS format works!', `\nCJS format works!\n`\)/);

    // Run again without update flag -> should still pass.
    const third = await common.spawnPromisified(
      process.execPath,
      ['--test', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(third.code, 0);
    assertSummary(t, third.stdout, { tests: 1, pass: 1, fail: 0 });
  });

  test('ES Module (.mjs) file support', async (t) => {
    tmpdir.refresh();
    const testFile = writeTestFile('inline/esmodule.mjs', fixtureSources['inline/esmodule.mjs']);

    // Run without update flag -> should fail, missing inline snapshots.
    const first = await common.spawnPromisified(
      process.execPath,
      ['--test', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(first.code, 1);
    t.assert.match(first.stdout, /Missing inline snapshots/i);
    assertSummary(t, first.stdout, { tests: 1, pass: 0, fail: 1 });

    // Run with update flag -> generates inline snapshots and passes.
    const second = await common.spawnPromisified(
      process.execPath,
      ['--test', '--test-update-snapshots', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(second.code, 0);
    assertSummary(t, second.stdout, { tests: 1, pass: 1, fail: 0 });

    // Verify file was mutated with inline template literals.
    const updatedSource = fs.readFileSync(testFile, 'utf8');
    t.assert.match(updatedSource, /inlineSnapshot\(data, `\n[\s\S]*\n`\)/);
    t.assert.match(updatedSource, /inlineSnapshot\('MJS format works!', `\nMJS format works!\n`\)/);

    // Run again without update flag -> should still pass.
    const third = await common.spawnPromisified(
      process.execPath,
      ['--test', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(third.code, 0);
    assertSummary(t, third.stdout, { tests: 1, pass: 1, fail: 0 });
  });

  test('mixed syntax parsing (script fallback)', async (t) => {
    tmpdir.refresh();
    const testFile = writeTestFile('inline/mixed-syntax.js', fixtureSources['inline/mixed-syntax.js']);

    // Run without update flag -> should fail, missing inline snapshots.
    const first = await common.spawnPromisified(
      process.execPath,
      ['--test', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(first.code, 1);
    t.assert.match(first.stdout, /Missing inline snapshots/i);
    assertSummary(t, first.stdout, { tests: 1, pass: 0, fail: 1 });

    // Run with update flag -> generates inline snapshots and passes.
    const second = await common.spawnPromisified(
      process.execPath,
      ['--test', '--test-update-snapshots', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(second.code, 0);
    assertSummary(t, second.stdout, { tests: 1, pass: 1, fail: 0 });

    // Verify file was mutated with inline template literals.
    const updatedSource = fs.readFileSync(testFile, 'utf8');
    t.assert.match(updatedSource, /inlineSnapshot\(data, `\n[\s\S]*\n`\)/);
    t.assert.match(updatedSource, /inlineSnapshot\('Mixed syntax handled correctly', `\nMixed syntax handled correctly\n`\)/);

    // Run again without update flag -> should still pass.
    const third = await common.spawnPromisified(
      process.execPath,
      ['--test', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(third.code, 0);
    assertSummary(t, third.stdout, { tests: 1, pass: 1, fail: 0 });
  });

  test('direct call pattern (destructured from t.assert)', async (t) => {
    tmpdir.refresh();
    const testFile = writeTestFile('inline/direct-call.js', fixtureSources['inline/direct-call.js']);

    // Run without update flag -> should fail, missing inline snapshots.
    const first = await common.spawnPromisified(
      process.execPath,
      ['--test', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(first.code, 1);
    t.assert.match(first.stdout, /Missing inline snapshots/i);
    assertSummary(t, first.stdout, { tests: 1, pass: 0, fail: 1 });

    // Run with update flag -> generates inline snapshots and passes.
    const second = await common.spawnPromisified(
      process.execPath,
      ['--test', '--test-update-snapshots', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(second.code, 0);
    assertSummary(t, second.stdout, { tests: 1, pass: 1, fail: 0 });

    // Verify file was mutated with inline template literals.
    const updatedSource = fs.readFileSync(testFile, 'utf8');
    t.assert.match(updatedSource, /inlineSnapshot\(result, `\n[\s\S]*\n`\)/);
    t.assert.match(updatedSource, /inlineSnapshot\('Direct call pattern test', `\nDirect call pattern test\n`\)/);

    // Run again without update flag -> should still pass.
    const third = await common.spawnPromisified(
      process.execPath,
      ['--test', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(third.code, 0);
    assertSummary(t, third.stdout, { tests: 1, pass: 1, fail: 0 });
  });

  test.todo('renamed inlineSnapshot should be detected as direct call', async (t) => {
    tmpdir.refresh();
    const testFile = writeTestFile('inline/renamed-call.js', fixtureSources['inline/renamed-call.js']);

    // Run without update flag -> should fail for ALL calls (2 renamed + 1 member expression).
    const first = await common.spawnPromisified(
      process.execPath,
      ['--test', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(first.code, 1);
    t.assert.match(first.stdout, /Missing inline snapshots/i);
    // Should detect all 3 snapshots (2 renamed calls + 1 member expression)
    assertSummary(t, first.stdout, { tests: 1, pass: 0, fail: 1 });

    // Run with update flag -> all calls should get snapshots.
    const second = await common.spawnPromisified(
      process.execPath,
      ['--test', '--test-update-snapshots', testFile],
      { cwd: tmpdir.path },
    );
    t.assert.strictEqual(second.code, 0);
    assertSummary(t, second.stdout, { tests: 1, pass: 1, fail: 0 });

    // Verify ALL calls were updated with template literals.
    const updatedSource = fs.readFileSync(testFile, 'utf8');
    t.assert.match(updatedSource, /t\.assert\.inlineSnapshot\(\{ fallback: "member expression" \}, `\n[\s\S]*\n`\)/);
    // The renamed calls should also get template literals added
    t.assert.match(updatedSource, /snap\(result, `\n[\s\S]*\n`\)/);
    t.assert.match(updatedSource, /snap\('Renamed call pattern test', `\nRenamed call pattern test\n`\)/);
  });
});
