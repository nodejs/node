// Flags: --no-warnings
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert/strict';
import { register } from 'node:module';
import { MessageChannel } from 'node:worker_threads';

const { port1, port2 } = new MessageChannel();

register(fixtures.fileURL('es-module-loaders/mock-loader.mjs'), {
  parentURL: import.meta.url,
  data: { port: port2 },
  transferList: [port2],
});

/**
 * This is the Map that saves *all* the mocked URL -> replacement Module
 * mappings
 * @type {Map<string, {namespace, listeners}>}
 */
globalThis.mockedModules = new Map();
let mockVersion = 0;

/**
 * @param {string} resolved an absolute URL HREF string
 * @param {object} replacementProperties an object to pick properties from
 *                                       to act as a module namespace
 * @returns {object} a mutator object that can update the module namespace
 *                   since we can't do something like old Object.observe
 */
function mock(resolved, replacementProperties) {
  const exportNames = Object.keys(replacementProperties);
  const namespace = { __proto__: null };
  /**
   * @type {Array<(name: string)=>void>} functions to call whenever an
   *                                     export name is updated
   */
  const listeners = [];
  for (const name of exportNames) {
    let currentValueForPropertyName = replacementProperties[name];
    Object.defineProperty(namespace, name, {
      enumerable: true,
      get() {
        return currentValueForPropertyName;
      },
      set(v) {
        currentValueForPropertyName = v;
        for (const fn of listeners) {
          try {
            fn(name);
          } catch {
            /* noop */
          }
        }
      }
    });
  }
  globalThis.mockedModules.set(encodeURIComponent(resolved), {
    namespace,
    listeners
  });
  mockVersion++;
  // Inform the loader that the `resolved` URL should now use the specific
  // `mockVersion` and has export names of `exportNames`
  //
  // This allows the loader to generate a fake module for that version
  // and names the next time it resolves a specifier to equal `resolved`
  port1.postMessage({ mockVersion, resolved, exports: exportNames });
  return namespace;
}

mock('node:events', {
  EventEmitter: 'This is mocked!'
});

// This resolves to node:events
// It is intercepted by mock-loader and doesn't return the normal value
assert.deepStrictEqual(await import('events'), Object.defineProperty({
  __proto__: null,
  EventEmitter: 'This is mocked!'
}, Symbol.toStringTag, {
  enumerable: false,
  value: 'Module'
}));

const mutator = mock('node:events', {
  EventEmitter: 'This is mocked v2!'
});

// It is intercepted by mock-loader and doesn't return the normal value.
// This is resolved separately from the import above since the specifiers
// are different.
const mockedV2 = await import('node:events');
assert.deepStrictEqual(mockedV2, Object.defineProperty({
  __proto__: null,
  EventEmitter: 'This is mocked v2!'
}, Symbol.toStringTag, {
  enumerable: false,
  value: 'Module'
}));

mutator.EventEmitter = 'This is mocked v3!';
assert.deepStrictEqual(mockedV2, Object.defineProperty({
  __proto__: null,
  EventEmitter: 'This is mocked v3!'
}, Symbol.toStringTag, {
  enumerable: false,
  value: 'Module'
}));

delete globalThis.mockedModules;
