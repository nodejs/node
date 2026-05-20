import { skip, spawnPromisified } from '../common/index.mjs';
import assert from 'node:assert';
import { test } from 'node:test';

if (!process.config.variables.node_use_amaro) skip('Requires Amaro');

test('eval TypeScript ESM syntax', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--eval',
    `import util from 'node:util'
    const text: string = 'Hello, TypeScript!'
    console.log(util.styleText('red', text));`]);

  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});

test('eval TypeScript ESM syntax with input-type module', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--input-type=module-typescript',
    '--eval',
    `import util from 'node:util'
    const text: string = 'Hello, TypeScript!'
    console.log(util.styleText('red', text));`]);

  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});

test('eval TypeScript CommonJS syntax', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--eval',
    `const util = require('node:util');
    const text: string = 'Hello, TypeScript!'
    console.log(util.styleText('red', text));`]);
  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.stderr, '');
  assert.strictEqual(result.code, 0);
});

test('eval TypeScript CommonJS syntax with input-type commonjs-typescript', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--input-type=commonjs-typescript',
    '--eval',
    `const util = require('node:util');
    const text: string = 'Hello, TypeScript!'
    console.log(util.styleText('red', text));`,
    '--no-warnings']);
  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.stderr, '');
  assert.strictEqual(result.code, 0);
});

test('eval TypeScript CommonJS syntax by default', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--eval',
    `const util = require('node:util');
    const text: string = 'Hello, TypeScript!'
    console.log(util.styleText('red', text));`,
    '--no-warnings']);

  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});

test('TypeScript ESM syntax not specified', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--eval',
    `import util from 'node:util'
    const text: string = 'Hello, TypeScript!'
    console.log(text);`]);
  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});

test('expect fail eval TypeScript CommonJS syntax with input-type module-typescript', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--input-type=module-typescript',
    '--eval',
    `const util = require('node:util');
    const text: string = 'Hello, TypeScript!'
    console.log(util.styleText('red', text));`]);

  assert.strictEqual(result.stdout, '');
  assert.match(result.stderr, /require is not defined in ES module scope, you can use import instead/);
  assert.strictEqual(result.code, 1);
});

test('expect fail eval TypeScript ESM syntax with input-type commonjs-typescript', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--input-type=commonjs-typescript',
    '--eval',
    `import util from 'node:util'
    const text: string = 'Hello, TypeScript!'
    console.log(util.styleText('red', text));`]);
  assert.strictEqual(result.stdout, '');
  assert.match(result.stderr, /Cannot use import statement outside a module/);
  assert.strictEqual(result.code, 1);
});

test('check syntax error is thrown when passing unsupported syntax', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--eval',
    'enum Foo { A, B, C }']);
  assert.strictEqual(result.stdout, '');
  assert.match(result.stderr, /SyntaxError/);
  assert.doesNotMatch(result.stderr, /ERR_UNSUPPORTED_TYPESCRIPT_SYNTAX/);
  assert.strictEqual(result.code, 1);
});

test('check syntax error is thrown when passing unsupported syntax with --input-type=module-typescript', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--input-type=module-typescript',
    '--eval',
    'enum Foo { A, B, C }']);
  assert.strictEqual(result.stdout, '');
  assert.match(result.stderr, /ERR_UNSUPPORTED_TYPESCRIPT_SYNTAX/);
  assert.strictEqual(result.code, 1);
});

test('check syntax error is thrown when passing unsupported syntax with --input-type=commonjs-typescript', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--input-type=commonjs-typescript',
    '--eval',
    'enum Foo { A, B, C }']);
  assert.strictEqual(result.stdout, '');
  assert.match(result.stderr, /ERR_UNSUPPORTED_TYPESCRIPT_SYNTAX/);
  assert.strictEqual(result.code, 1);
});

test('should not parse TypeScript with --type-module=commonjs', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--input-type=commonjs',
    '--eval',
    `enum Foo {}`]);

  assert.strictEqual(result.stdout, '');
  assert.match(result.stderr, /SyntaxError/);
  assert.doesNotMatch(result.stderr, /ERR_UNSUPPORTED_TYPESCRIPT_SYNTAX/);
  assert.strictEqual(result.code, 1);
});

test('should not parse TypeScript with --type-module=module', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--input-type=module',
    '--eval',
    `enum Foo {}`]);

  assert.strictEqual(result.stdout, '');
  assert.match(result.stderr, /SyntaxError/);
  assert.doesNotMatch(result.stderr, /ERR_UNSUPPORTED_TYPESCRIPT_SYNTAX/);
  assert.strictEqual(result.code, 1);
});

test('check warning is emitted when eval TypeScript CommonJS syntax', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--eval',
    `const util = require('node:util');
    const text: string = 'Hello, TypeScript!'
    console.log(util.styleText('red', text));`]);
  assert.strictEqual(result.stderr, '');
  assert.match(result.stdout, /Hello, TypeScript!/);
  assert.strictEqual(result.code, 0);
});

test('code is throwing a non Error is rethrown', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--eval',
    `throw null;`]);
  assert.doesNotMatch(result.stderr, /node:internal\/process\/execution/);
  assert.strictEqual(result.stdout, '');
  assert.strictEqual(result.code, 1);
});

test('code is throwing an error with customized accessors', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--eval',
    `throw Object.defineProperty(new Error, "stack", { set() {throw this} });`]);

  assert.match(result.stderr, /Error/);
  assert.match(result.stderr, /at \[eval\]:1:29/);
  assert.strictEqual(result.stdout, '');
  assert.strictEqual(result.code, 1);
});

test('typescript code is throwing an error', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--eval',
    `const foo: string =  'Hello, TypeScript!'; throw new Error(foo);`]);

  assert.match(result.stderr, /Hello, TypeScript!/);
  assert.strictEqual(result.stdout, '');
  assert.strictEqual(result.code, 1);
});

test('typescript ESM code is throwing a syntax error at runtime', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--eval',
    'import util from "node:util"; function foo(){}; throw new SyntaxError(foo<Number>(1));']);
  // Trick by passing ambiguous syntax to see if evaluated in TypeScript or JavaScript
  // If evaluated in JavaScript `foo<Number>(1)` is evaluated as `foo < Number > (1)`
  // result in false
  // If evaluated in TypeScript `foo<Number>(1)` is evaluated as `foo(1)`
  assert.match(result.stderr, /SyntaxError: false/);
  assert.strictEqual(result.stdout, '');
  assert.strictEqual(result.code, 1);
});

test('typescript CJS code is throwing a syntax error at runtime', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--eval',
    'const util = require("node:util"); function foo(){}; throw new SyntaxError(foo<Number>(1));']);
  // Trick by passing ambiguous syntax to see if evaluated in TypeScript or JavaScript
  // If evaluated in JavaScript `foo<Number>(1)` is evaluated as `foo < Number > (1)`
  // result in false
  // If evaluated in TypeScript `foo<Number>(1)` is evaluated as `foo(1)`
  assert.match(result.stderr, /SyntaxError: false/);
  assert.strictEqual(result.stdout, '');
  assert.strictEqual(result.code, 1);
});

test('check syntax error is thrown when passing invalid syntax with --input-type=commonjs-typescript', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--input-type=commonjs-typescript',
    '--eval',
    'function foo(){ await Promise.resolve(1); }']);
  assert.strictEqual(result.stdout, '');
  assert.match(result.stderr, /ERR_INVALID_TYPESCRIPT_SYNTAX/);
  assert.strictEqual(result.code, 1);
});

test('check syntax error is thrown when passing invalid syntax with --input-type=module-typescript', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--input-type=module-typescript',
    '--eval',
    'function foo(){ await Promise.resolve(1); }']);
  assert.strictEqual(result.stdout, '');
  assert.match(result.stderr, /ERR_INVALID_TYPESCRIPT_SYNTAX/);
  assert.strictEqual(result.code, 1);
});

test('should not allow module keyword', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--input-type=module-typescript',
    '--eval',
    'module F { export type x = number }']);
  assert.strictEqual(result.stdout, '');
  assert.match(result.stderr, /ERR_UNSUPPORTED_TYPESCRIPT_SYNTAX/);
  assert.strictEqual(result.code, 1);
});

test('should not allow declare module keyword', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--input-type=module-typescript',
    '--eval',
    'declare module F { export type x = number }']);
  assert.strictEqual(result.stdout, '');
  assert.match(result.stderr, /ERR_UNSUPPORTED_TYPESCRIPT_SYNTAX/);
  assert.strictEqual(result.code, 1);
});

// TODO (marco-ippolito) Remove the extra padding from the error message
// The padding comes from swc it will be removed in a future amaro release
test('the error message should not contain extra padding', async () => {
  const result = await spawnPromisified(process.execPath, [
    '--input-type=module-typescript',
    '--eval',
    'declare module F { export type x = number }']);
  assert.strictEqual(result.stdout, '');
  // Windows uses \r\n as line endings
  const lines = result.stderr.replace(/\r\n/g, '\n').split('\n');
  assert.strictEqual(lines[0], '[eval]:1');
  assert.strictEqual(lines[1], 'declare module F { export type x = number }');
  assert.strictEqual(lines[2], '        ^^^^^^^^');
  assert.strictEqual(lines[4], 'SyntaxError [ERR_UNSUPPORTED_TYPESCRIPT_SYNTAX]:' +
    ' `module` keyword is not supported. Use `namespace` instead.');
  assert.strictEqual(result.code, 1);
});
