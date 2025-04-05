import { skip } from '../common/index.mjs';
import { test } from 'node:test';
import { detectSyntax } from 'node:module';
import { strictEqual, throws } from 'node:assert';

if (!process.config.variables.node_use_amaro) skip('Requires Amaro');

test('basic input validation', () => {
  const cases = [{}, undefined, 5, null, false, Symbol('foo'), [], () => { }];

  for (const value of cases) {
    throws(() => detectSyntax(value), {
      name: 'TypeError',
    });
  }
});

test('detectSyntax', () => {
  const cases = [
    { source: `const x = require('fs');`, expected: 'commonjs' },
    { source: `import fs from 'fs';`, expected: 'module' },
    { source: `const x: number = 5;`, expected: 'commonjs-typescript' },
    { source: `interface User { name: string; }`, expected: 'commonjs-typescript' },
    { source: `import fs from 'fs'; const x: number = 5;`, expected: 'module-typescript' },
    { source: `const x: number = 5;`, expected: 'commonjs-typescript' },
    { source: `function add(a: number, b: number) { return a + b; }`, expected: 'commonjs-typescript' },
    { source: `type X = unknown; function fail(): X { return 5; }`, expected: 'commonjs-typescript' },
    { source: `const x: never = 5;`, expected: 'commonjs-typescript' },
    { source: `import foo from "bar"; const foo: string = "bar";`, expected: 'module-typescript' },
    { source: `const foo: string = "bar";`, expected: 'commonjs-typescript' },
    { source: `import foo from "bar";`, expected: 'module' },
    { source: `const foo = "bar";`, expected: 'commonjs' },
    { source: `module.exports = {};`, expected: 'commonjs' },
    { source: `exports.foo = 42;`, expected: 'commonjs' },
    { source: `export default function () {};`, expected: 'module' },
    { source: `export const foo = 42;`, expected: 'module' },
    { source: `const x: number = 5;`, expected: 'commonjs-typescript' },
    { source: `interface User { name: string; }`, expected: 'commonjs-typescript' },
    { source: `import fs from 'fs'; const x: number = 5;`, expected: 'module-typescript' },
    { source: `type X = unknown; function fail(): X { return 5; }`, expected: 'commonjs-typescript' },
    { source: `foo<Object>("bar");`, expected: 'commonjs-typescript' },
    { source: `foo<T, U>(arg);`, expected: 'commonjs-typescript' },
    { source: `<div>hello</div>;`, expected: undefined },
    { source: `const el = <span>{foo}</span>;`, expected: undefined },
    { source: `/** @type {string} */ let foo;`, expected: 'commonjs' },
    { source: `/** @param {number} x */ function square(x) { return x * x; }`, expected: 'commonjs' },
    { source: `function foo(x: number): string { return x.toString(); }`, expected: 'commonjs-typescript' },
    // Decorators are ignored by the TypeScript parser
    { source: `@Component class MyComponent {}`, expected: 'commonjs' },
    { source: `const x: never = 5;`, expected: 'commonjs-typescript' },
    { source: `import type { Foo } from './types';`, expected: 'module-typescript' },
    { source: `import { type Foo } from './types';`, expected: 'module-typescript' },
    { source: '', expected: 'commonjs' },
    { source: ' ', expected: 'commonjs' },
    { source: '\n\n', expected: 'commonjs' },
    { source: `const foo;`, expected: undefined },
    // // This is an edge case where the parser detects syntax wrong
    { source: `fetch<Object>("boo")`, expected: 'commonjs-typescript' },
    { source: `import x = require('x')`, expected: undefined },
  ];

  for (const { source, expected } of cases) {
    strictEqual(detectSyntax(source), expected);
  }
});
