import '../common/index.mjs';
import { rejects } from 'assert';

const fixtureBase = '../fixtures/es-modules/package-cjs-named-error';

const errTemplate = (specifier, name, namedImports, defaultName) =>
  `Named export '${name}' not found. The requested module` +
  ` '${specifier}' is a CommonJS module, which may not support ` +
  'all module.exports as named exports.\nCommonJS modules can ' +
  'always be imported via the default export, for example using:' +
  `\n\nimport ${defaultName || 'pkg'} from '${specifier}';\n` + (namedImports ?
    `const ${namedImports} = ${defaultName || 'pkg'};\n` : '');

const expectedMultiLine = errTemplate('./fail.cjs', 'comeOn',
                                      '{ comeOn, everybody }');

const expectedLongMultiLine = errTemplate('./fail.cjs', 'one',
                                          '{\n' +
                                          '  comeOn,\n' +
                                          '  one,\n' +
                                          '  two,\n' +
                                          '  three,\n' +
                                          '  four,\n' +
                                          '  five,\n' +
                                          '  six,\n' +
                                          '  seven,\n' +
                                          '  eight,\n' +
                                          '  nine,\n' +
                                          '  ten\n' +
                                          '}');

const expectedRelative = errTemplate('./fail.cjs', 'comeOn', '{ comeOn }');

const expectedRenamed = errTemplate('./fail.cjs', 'comeOn',
                                    '{ comeOn: comeOnRenamed }');

const expectedDefaultRenamed =
  errTemplate('./fail.cjs', 'everybody', '{ comeOn: comeOnRenamed, everybody }',
              'abc');

const expectedPackageHack =
    errTemplate('./json-hack/fail.js', 'comeOn', '{ comeOn }');

const expectedBare = errTemplate('deep-fail', 'comeOn', '{ comeOn }');

rejects(async () => {
  await import(`${fixtureBase}/single-quote.mjs`);
}, {
  name: 'SyntaxError',
  message: expectedRelative
}, 'should support relative specifiers with single quotes');

rejects(async () => {
  await import(`${fixtureBase}/double-quote.mjs`);
}, {
  name: 'SyntaxError',
  message: expectedRelative
}, 'should support relative specifiers with double quotes');

rejects(async () => {
  await import(`${fixtureBase}/renamed-import.mjs`);
}, {
  name: 'SyntaxError',
  message: expectedRenamed
}, 'should correctly format named imports with renames');

rejects(async () => {
  await import(`${fixtureBase}/default-and-renamed-import.mjs`);
}, {
  name: 'SyntaxError',
  message: expectedDefaultRenamed
}, 'should correctly format hybrid default and named imports with renames');

rejects(async () => {
  await import(`${fixtureBase}/multi-line.mjs`);
}, {
  name: 'SyntaxError',
  message: expectedMultiLine,
}, 'should correctly format named multi-line imports');

rejects(async () => {
  await import(`${fixtureBase}/long-multi-line.mjs`);
}, {
  name: 'SyntaxError',
  message: expectedLongMultiLine,
}, 'should correctly format named very long multi-line imports');

rejects(async () => {
  await import(`${fixtureBase}/json-hack.mjs`);
}, {
  name: 'SyntaxError',
  message: expectedPackageHack
}, 'should respect recursive package.json for module type');

rejects(async () => {
  await import(`${fixtureBase}/bare-import-single.mjs`);
}, {
  name: 'SyntaxError',
  message: expectedBare
}, 'should support bare specifiers with single quotes');

rejects(async () => {
  await import(`${fixtureBase}/bare-import-double.mjs`);
}, {
  name: 'SyntaxError',
  message: expectedBare
}, 'should support bare specifiers with double quotes');

rejects(async () => {
  await import(`${fixtureBase}/escaped-single-quote.mjs`);
}, /import pkg from '\.\/oh'no\.cjs'/, 'should support relative specifiers with escaped single quote');
