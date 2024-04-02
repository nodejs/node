import { register } from 'node:module';
import { MessageChannel } from 'node:worker_threads';


const { port1, port2 } = new MessageChannel();

register('./mock-loader.mjs', import.meta.url, {
  data: {
    port: port2,
    mainImportURL: import.meta.url,
  },
  transferList: [port2],
});

/**
 * This is the Map that saves *all* the mocked URL -> replacement Module
 * mappings
 * @type {Map<string, {namespace, listeners}>}
 */
export const mockedModules = new Map();
let mockVersion = 0;

/**
 * @param {string} resolved an absolute URL HREF string
 * @param {object} replacementProperties an object to pick properties from
 *                                       to act as a module namespace
 * @returns {object} a mutator object that can update the module namespace
 *                   since we can't do something like old Object.observe
 */
export function mock(resolved, replacementProperties) {
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
      __proto__: null,
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
      },
    });
  }
  mockedModules.set(encodeURIComponent(resolved), {
    namespace,
    listeners,
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
