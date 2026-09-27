import { spawnPromisified } from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'assert';
import { execPath } from 'node:process';

const fixtureBase = '../fixtures/es-modules/package-cjs-named-error';

const errTemplate = (specifier, name, namedImports) =>
  `Named export '${name}' not found. The requested module` +
  ` '${specifier}' is a CommonJS module, which may not support ` +
  'all module.exports as named exports.\nCommonJS modules can ' +
  'always be imported via the default export, for example using:' +
  `\n\nimport pkg from '${specifier}';\n` + (namedImports ?
    `const ${namedImports} = pkg;\n` : '');

const expectedWithoutExample = errTemplate('./fail.cjs', 'comeOn');

const expectedRelative = errTemplate('./fail.cjs', 'comeOn', '{ comeOn }');

const expectedRenamed = errTemplate('./fail.cjs', 'comeOn',
                                    '{ comeOn: comeOnRenamed }');

const expectedPackageHack =
    errTemplate('./json-hack/fail.js', 'comeOn', '{ comeOn }');

const expectedBare = errTemplate('deep-fail', 'comeOn', '{ comeOn }');

await assert.rejects(async () => {
  await import(`${fixtureBase}/single-quote.mjs`);
}, {
  name: 'SyntaxError',
  message: expectedRelative,
}, 'should support relative specifiers with single quotes');

await assert.rejects(async () => {
  await import(`${fixtureBase}/double-quote.mjs`);
}, {
  name: 'SyntaxError',
  message: expectedRelative,
}, 'should support relative specifiers with double quotes');

await assert.rejects(async () => {
  await import(`${fixtureBase}/renamed-import.mjs`);
}, {
  name: 'SyntaxError',
  message: expectedRenamed,
}, 'should correctly format named imports with renames');

await assert.rejects(async () => {
  await import(`${fixtureBase}/multi-line.mjs`);
}, {
  name: 'SyntaxError',
  message: expectedWithoutExample,
}, 'should correctly format named imports across multiple lines');

await assert.rejects(async () => {
  await import(`${fixtureBase}/json-hack.mjs`);
}, {
  name: 'SyntaxError',
  message: expectedPackageHack,
}, 'should respect recursive package.json for module type');

await assert.rejects(async () => {
  await import(`${fixtureBase}/bare-import-single.mjs`);
}, {
  name: 'SyntaxError',
  message: expectedBare,
}, 'should support bare specifiers with single quotes');

await assert.rejects(async () => {
  await import(`${fixtureBase}/bare-import-double.mjs`);
}, {
  name: 'SyntaxError',
  message: expectedBare,
}, 'should support bare specifiers with double quotes');

await assert.rejects(async () => {
  await import(`${fixtureBase}/escaped-single-quote.mjs`);
}, /import pkg from '\.\/oh'no\.cjs'/, 'should support relative specifiers with escaped single quote');

const entryPoint = fixtures.path('es-modules', 'package-cjs-named-error', 'single-quote.mjs');
const { code, stderr } = await spawnPromisified(execPath, [entryPoint]);
assert.strictEqual(code, 1);
assert.ok(stderr.includes("Named export 'comeOn' not found."),
          'entry point should show the missing named export');
assert.ok(stderr.includes('CommonJS modules can always be imported via the default export'),
          'entry point should show the CommonJS named export hint');
assert.ok(stderr.includes("import pkg from './fail.cjs';"),
          'entry point hint should recommend the default import');
assert.ok(stderr.includes('const { comeOn } = pkg;'),
          'entry point hint should show the named import as destructuring');
assert.ok(stderr.includes("import { comeOn } from './fail.cjs';"),
          'entry point error should include the source import statement');
