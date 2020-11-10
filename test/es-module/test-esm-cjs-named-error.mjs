import '../common/index.mjs';
import { rejects } from 'assert';

const fixtureBase = '../fixtures/es-modules/package-cjs-named-error';

const expectedRelative = 'The requested module \'./fail.cjs\' is expected to ' +
  'be of type CommonJS, which does not support named exports. CommonJS ' +
  'modules can be imported by importing the default export.\n' +
  'For example:\n' +
  'import pkg from \'./fail.cjs\';\n' +
  'const { comeOn } = pkg;';

const expectedRenamed = 'The requested module \'./fail.cjs\' is expected to ' +
  'be of type CommonJS, which does not support named exports. CommonJS ' +
  'modules can be imported by importing the default export.\n' +
  'For example:\n' +
  'import pkg from \'./fail.cjs\';\n' +
  'const { comeOn: comeOnRenamed } = pkg;';

const expectedPackageHack = 'The requested module \'./json-hack/fail.js\' is ' +
  'expected to be of type CommonJS, which does not support named exports. ' +
  'CommonJS modules can be imported by importing the default export.\n' +
  'For example:\n' +
  'import pkg from \'./json-hack/fail.js\';\n' +
  'const { comeOn } = pkg;';

const expectedBare = 'The requested module \'deep-fail\' is expected to ' +
  'be of type CommonJS, which does not support named exports. CommonJS ' +
  'modules can be imported by importing the default export.\n' +
  'For example:\n' +
  'import pkg from \'deep-fail\';\n' +
  'const { comeOn } = pkg;';

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
