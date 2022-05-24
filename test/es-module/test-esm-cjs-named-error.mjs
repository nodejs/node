import '../common/index.mjs';
import { rejects } from 'assert';

const fixtureBase = '../fixtures/es-modules/package-cjs-named-error';

rejects(async () => {
  await import(`${fixtureBase}/single-quote.mjs`);
}, {
  name: 'SyntaxError',
}, 'should support relative specifiers with single quotes');

rejects(async () => {
  await import(`${fixtureBase}/double-quote.mjs`);
}, {
  name: 'SyntaxError',
}, 'should support relative specifiers with double quotes');

rejects(async () => {
  await import(`${fixtureBase}/renamed-import.mjs`);
}, {
  name: 'SyntaxError',
}, 'should correctly format named imports with renames');

rejects(async () => {
  await import(`${fixtureBase}/multi-line.mjs`);
}, {
  name: 'SyntaxError',
}, 'should correctly format named imports across multiple lines');

rejects(async () => {
  await import(`${fixtureBase}/json-hack.mjs`);
}, {
  name: 'SyntaxError',
}, 'should respect recursive package.json for module type');

rejects(async () => {
  await import(`${fixtureBase}/bare-import-single.mjs`);
}, {
  name: 'SyntaxError',
}, 'should support bare specifiers with single quotes');

rejects(async () => {
  await import(`${fixtureBase}/bare-import-double.mjs`);
}, {
  name: 'SyntaxError',
}, 'should support bare specifiers with double quotes');

rejects(async () => {
  await import(`${fixtureBase}/escaped-single-quote.mjs`);
}, /import pkg from '\.\/oh'no\.cjs'/, 'should support relative specifiers with escaped single quote');
