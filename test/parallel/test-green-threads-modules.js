// Flags: --experimental-green-threads --no-warnings
'use strict';

// ES modules in green threads: the bundle, the host's resolve() and load()
// hooks, import(), import.meta, and the main module's exports.

const common = require('../common');
const assert = require('assert');
const { once } = require('events');
const { GreenThread, RpcTarget } = require('node:green_threads');

// Resolves with the first 'error' event, and checks that `exports` rejects
// with it too.
async function mainModuleError(greenThread) {
  const [[error]] = await Promise.all([
    once(greenThread, 'error'),
    assert.rejects(greenThread.exports, (rejection) => rejection.name !== undefined),
  ]);
  assert.strictEqual(await once(greenThread, 'exit').then(([code]) => code), 1);
  return error;
}

(async () => {
  // The bundle, JSON modules, cycles, import.meta, import() and exports.
  {
    const resolved = [];
    const greenThread = new GreenThread(`
      import { add } from './lib/math.js';
      import config from './config.json' with { type: 'json' };
      import { a } from './cycle-a.js';
      import { greet } from 'greeter';
      export const url = import.meta.url;
      export const { name } = config;
      export { add, a };
      export function hello(who) { return greet(who); }
      export async function dynamic(specifier) {
        try {
          const namespace = await import(specifier);
          return Object.keys(namespace);
        } catch (error) {
          return error.code;
        }
      }
      export function counter() {
        let count = 0;
        return new (class extends RpcTarget { increment() { return ++count; } })();
      }
      await new Promise((resolve) => setTimeout(resolve, 1));
    `, {
      type: 'module',
      filename: 'file:///app/main.js',
      modules: {
        'file:///app/lib/math.js': `
          import { one } from '../one.js';
          export const add = (x, y) => x + y + one - 1;
        `,
        'file:///app/one.js': 'export const one = 1;',
        'file:///app/config.json': { source: '{"name":"app"}', format: 'json' },
        'file:///app/cycle-a.js':
          'import { b } from "./cycle-b.js"; export const a = "a" + b;',
        'file:///app/cycle-b.js':
          'import "./cycle-a.js"; export const b = "b";',
      },
      resolve(specifier, context) {
        resolved.push([specifier, context.referrer]);
        if (specifier === 'greeter') {
          return { url: 'https://example.com/greeter.js' };
        }
      },
      async load(url) {
        if (url === 'https://example.com/greeter.js') {
          return {
            source: new TextEncoder().encode(
              'export const greet = (who) => "hi " + who;'),
          };
        }
      },
    });
    const exports = await greenThread.exports;
    assert.deepStrictEqual(Object.keys(exports).sort(), [
      'a', 'add', 'counter', 'dynamic', 'hello', 'name', 'url',
    ]);
    assert.strictEqual(exports.url, 'file:///app/main.js');
    assert.strictEqual(exports.name, 'app');
    assert.strictEqual(exports.a, 'ab');
    assert.strictEqual(await exports.add(1, 2), 3);
    assert.strictEqual(await exports.hello('you'), 'hi you');
    assert.deepStrictEqual(await exports.dynamic('./one.js'), ['one']);
    assert.strictEqual(await exports.dynamic('node:fs'),
                       'ERR_UNSUPPORTED_ESM_URL_SCHEME');
    assert.strictEqual(await exports.dynamic('missing'),
                       'ERR_MODULE_NOT_FOUND');
    assert.strictEqual(await exports.dynamic('./missing.js'),
                       'ERR_MODULE_NOT_FOUND');
    const counter = await exports.counter();
    assert.strictEqual(await counter.increment(), 1);
    assert.strictEqual(await counter.increment(), 2);
    // resolve() is only called for bare specifiers.
    assert.deepStrictEqual(resolved, [
      ['greeter', 'file:///app/main.js'],
      ['missing', 'file:///app/main.js'],
    ]);
    await greenThread.terminate();
  }

  // Absolute paths are file URLs.
  {
    const greenThread = new GreenThread(`
      export { value } from './value.js';
      export const url = import.meta.url;
    `, {
      type: 'module',
      filename: '/app/main.js',
      modules: { '/app/value.js': 'export const value = 1;' },
    });
    const { value, url } = await greenThread.exports;
    assert.strictEqual(value, 1);
    assert.strictEqual(url, 'file:///app/main.js');
    await greenThread.terminate();
  }

  // Scripts can import() modules, and have no exports.
  {
    const greenThread = new GreenThread(`
      import('file:///value.js').then(({ value }) => postMessage(value));
    `, { modules: { 'file:///value.js': 'export const value = 42;' } });
    assert.strictEqual(greenThread.exports, undefined);
    assert.deepStrictEqual(await once(greenThread, 'message'), [42]);
    await greenThread.terminate();
  }

  // Errors while loading or evaluating the main module.
  for (const [source, options, check] of [
    ['export const = 1;', {}, { name: 'SyntaxError' }],
    ['import "./missing.js";', {},
     { code: 'ERR_MODULE_NOT_FOUND',
       message: /^Cannot find module 'file:\/\/\/missing\.js'/ }],
    ['import "bare";', {},
     { code: 'ERR_MODULE_NOT_FOUND',
       message: "Cannot find module 'bare' imported from file:///main.js" }],
    ['import "node:fs";', {}, { code: 'ERR_UNSUPPORTED_ESM_URL_SCHEME' }],
    ['import "./data.json";',
     { modules: { 'file:///data.json': { source: '1', format: 'json' } } },
     { code: 'ERR_IMPORT_ATTRIBUTE_MISSING' }],
    ['import "./code.js" with { type: "json" };',
     { modules: { 'file:///code.js': '' } },
     { code: 'ERR_IMPORT_ATTRIBUTE_TYPE_INCOMPATIBLE' }],
    ['import "./code.js" with { type: "css" };',
     { modules: { 'file:///code.js': '' } },
     { code: 'ERR_IMPORT_ATTRIBUTE_UNSUPPORTED' }],
    ['import { nope } from "./code.js";',
     { modules: { 'file:///code.js': 'export const yes = 1;' } },
     { name: 'SyntaxError' }],
    ['await Promise.reject(new RangeError("top-level"));', {},
     { name: 'RangeError', message: 'top-level' }],
    ['import "bare";',
     { resolve() { throw new TypeError('resolve failed'); } },
     { name: 'TypeError', message: 'resolve failed' }],
    ['import "bare";', { resolve: () => ({ url: 'not a url' }) },
     { code: 'ERR_INVALID_RETURN_PROPERTY_VALUE',
       message: /"url" from the "resolve" hook/ }],
    ['import "./code.js";', { load: () => ({ source: 1 }) },
     { code: 'ERR_INVALID_RETURN_PROPERTY_VALUE',
       message: /"source" from the "load" hook/ }],
    ['import "./code.js";', { load: () => ({ source: '', format: 'wasm' }) },
     { code: 'ERR_INVALID_RETURN_PROPERTY_VALUE',
       message: /"format" from the "load" hook/ }],
  ]) {
    const greenThread = new GreenThread(source, {
      type: 'module',
      filename: 'file:///main.js',
      ...options,
    });
    const error = await mainModuleError(greenThread);
    for (const [key, expected] of Object.entries(check)) {
      if (expected instanceof RegExp) {
        assert.match(error[key], expected);
      } else {
        assert.strictEqual(error[key], expected, `${source}: ${error.stack}`);
      }
    }
  }

  // Each call of an export runs in an invocation context of its own, which
  // continues after the call returned.
  {
    const greenThread = new GreenThread(`
      export function later(value) {
        setTimeout(() => postMessage(value), 1);
        return 'returned';
      }
    `, { type: 'module' });
    const { later } = await greenThread.exports;
    assert.strictEqual(await later('timer'), 'returned');
    assert.deepStrictEqual(await once(greenThread, 'message'), ['timer']);
    await greenThread.terminate();
  }

  // Invalid options.
  assert.throws(() => new GreenThread('', { type: 'commonjs' }), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
  assert.throws(() => new GreenThread('', { type: 'module', filename: 'x.js' }), {
    code: 'ERR_INVALID_ARG_VALUE',
  });
  assert.throws(() => new GreenThread('', { modules: { 'file:///x.js': 1 } }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });
  assert.throws(() => new GreenThread('', {
    modules: { 'file:///x.js': { source: '', format: 'commonjs' } },
  }), { code: 'ERR_INVALID_ARG_TYPE' });
  assert.throws(() => new GreenThread('', { load: 1 }), {
    code: 'ERR_INVALID_ARG_TYPE',
  });

  assert.ok(RpcTarget);
})().then(common.mustCall());
