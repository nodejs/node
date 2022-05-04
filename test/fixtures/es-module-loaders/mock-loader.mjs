import { receiveMessageOnPort } from 'node:worker_threads';
const mockedModuleExports = new Map();
let currentMockVersion = 0;

// This loader causes a new module `node:mock` to become available as a way to
// swap module resolution results for mocking purposes. It uses this instead
// of import.meta so that CommonJS can still use the functionality.
//
// It does so by allowing non-mocked modules to live in normal URL cache
// locations but creates 'mock-facade:' URL cache location for every time a
// module location is mocked. Since a single URL can be mocked multiple
// times but it cannot be removed from the cache, `mock-facade:` URLs have a
// form of mock-facade:$VERSION:$REPLACING_URL with the parameters being URL
// percent encoded every time a module is resolved. So if a module for
// 'file:///app.js' is mocked it might look like
// 'mock-facade:12:file%3A%2F%2F%2Fapp.js'. This encoding is done to prevent
// problems like mocking URLs with special URL characters like '#' or '?' from
// accidentally being picked up as part of the 'mock-facade:' URL containing
// the mocked URL.
//
// NOTE: due to ESM spec, once a specifier has been resolved in a source text
//       it cannot be changed. So things like the following DO NOT WORK:
//
// ```mjs
// import mock from 'node:mock';
// mock('file:///app.js', {x:1});
// const namespace1 = await import('file:///app.js');
// namespace1.x; // 1
// mock('file:///app.js', {x:2});
// const namespace2 = await import('file:///app.js');
// namespace2.x; // STILL 1, because this source text already set the specifier
//               // for 'file:///app.js', a different specifier that resolves
//               // to that could still get a new namespace though
// assert(namespace1 === namespace2);
// ```

/**
 * FIXME: this is a hack to workaround loaders being
 * single threaded for now, just ensures that the MessagePort drains
 */
function doDrainPort() {
  let msg;
  while (msg = receiveMessageOnPort(preloadPort)) {
    onPreloadPortMessage(msg.message);
  }
}

/**
 * @param param0 message from the application context
 */
function onPreloadPortMessage({
  mockVersion, resolved, exports
}) {
  currentMockVersion = mockVersion;
  mockedModuleExports.set(resolved, exports);
}
let preloadPort;
export function globalPreload({port}) {
  // Save the communication port to the application context to send messages
  // to it later
  preloadPort = port;
  // Every time the application context sends a message over the port
  port.on('message', onPreloadPortMessage);
  // This prevents the port that the Loader/application talk over
  // from keeping the process alive, without this, an application would be kept
  // alive just because a loader is waiting for messages
  port.unref();

  const insideAppContext = (getBuiltin, port, setImportMetaCallback) => {
    /**
     * This is the Map that saves *all* the mocked URL -> replacement Module
     * mappings
     * @type {Map<string, {namespace, listeners}>}
     */
    let mockedModules = new Map();
    let mockVersion = 0;
    /**
     * This is the value that is placed into the `node:mock` default export
     *
     * @example
     * ```mjs
     * import mock from 'node:mock';
     * const mutator = mock('file:///app.js', {x:1});
     * const namespace = await import('file:///app.js');
     * namespace.x; // 1;
     * mutator.x = 2;
     * namespace.x; // 2;
     * ```
     *
     * @param {string} resolved an absolute URL HREF string
     * @param {object} replacementProperties an object to pick properties from
     *                                       to act as a module namespace
     * @returns {object} a mutator object that can update the module namespace
     *                   since we can't do something like old Object.observe
     */
    const doMock = (resolved, replacementProperties) => {
      let exportNames = Object.keys(replacementProperties);
      let namespace = Object.create(null);
      /**
       * @type {Array<(name: string)=>void>} functions to call whenever an
       *                                     export name is updated
       */
      let listeners = [];
      for (const name of exportNames) {
        let currentValueForPropertyName = replacementProperties[name];
        Object.defineProperty(namespace, name, {
          enumerable: true,
          get() {
            return currentValueForPropertyName;
          },
          set(v) {
            currentValueForPropertyName = v;
            for (let fn of listeners) {
              try {
                fn(name);
              } catch {
              }
            }
          }
        });
      }
      mockedModules.set(resolved, {
        namespace,
        listeners
      });
      mockVersion++;
      // Inform the loader that the `resolved` URL should now use the specific
      // `mockVersion` and has export names of `exportNames`
      //
      // This allows the loader to generate a fake module for that version
      // and names the next time it resolves a specifier to equal `resolved`
      port.postMessage({ mockVersion, resolved, exports: exportNames });
      return namespace;
    }
    // Sets the import.meta properties up
    // has the normal chaining workflow with `defaultImportMetaInitializer`
    setImportMetaCallback((meta, context, defaultImportMetaInitializer) => {
      /**
       * 'node:mock' creates its default export by plucking off of import.meta
       * and must do so in order to get the communications channel from inside
       * preloadCode
       */
      if (context.url === 'node:mock') {
        meta.doMock = doMock;
        return;
      }
      /**
       * Fake modules created by `node:mock` get their meta.mock utility set
       * to the corresponding value keyed off `mockedModules` and use this
       * to setup their exports/listeners properly
       */
      if (context.url.startsWith('mock-facade:')) {
        let [proto, version, encodedTargetURL] = context.url.split(':');
        let decodedTargetURL = decodeURIComponent(encodedTargetURL);
        if (mockedModules.has(decodedTargetURL)) {
          meta.mock = mockedModules.get(decodedTargetURL);
          return;
        }
      }
      /**
       * Ensure we still get things like `import.meta.url`
       */
      defaultImportMetaInitializer(meta, context);
    });
  };
  return `(${insideAppContext})(getBuiltin, port, setImportMetaCallback)`
}


// Rewrites node: loading to mock-facade: so that it can be intercepted
export async function resolve(specifier, context, defaultResolve) {
  if (specifier === 'node:mock') {
    return {
      shortCircuit: true,
      url: specifier
    };
  }
  doDrainPort();
  const def = await defaultResolve(specifier, context);
  if (context.parentURL?.startsWith('mock-facade:')) {
    // Do nothing, let it get the "real" module
  } else if (mockedModuleExports.has(def.url)) {
    return {
      shortCircuit: true,
      url: `mock-facade:${currentMockVersion}:${encodeURIComponent(def.url)}`
    };
  };
  return {
    shortCircuit: true,
    url: def.url,
  };
}

export async function load(url, context, defaultLoad) {
  doDrainPort();
  if (url === 'node:mock') {
    /**
     * Simply grab the import.meta.doMock to establish the communication
     * channel with preloadCode
     */
    return {
      shortCircuit: true,
      source: 'export default import.meta.doMock',
      format: 'module'
    };
  }
  /**
   * Mocked fake module, not going to be handled in default way so it
   * generates the source text, then short circuits
   */
  if (url.startsWith('mock-facade:')) {
    let [proto, version, encodedTargetURL] = url.split(':');
    let ret = generateModule(mockedModuleExports.get(
      decodeURIComponent(encodedTargetURL)
    ));
    return {
      shortCircuit: true,
      source: ret,
      format: 'module'
    };
  }
  return defaultLoad(url, context);
}

/**
 *
 * @param {Array<string>} exports name of the exports of the module
 * @returns {string}
 */
function generateModule(exports) {
  let body = [
    'export {};',
    'let mapping = {__proto__: null};'
  ];
  for (const [i, name] of Object.entries(exports)) {
    let key = JSON.stringify(name);
    body.push(`var _${i} = import.meta.mock.namespace[${key}];`);
    body.push(`Object.defineProperty(mapping, ${key}, { enumerable: true, set(v) {_${i} = v;}, get() {return _${i};} });`);
    body.push(`export {_${i} as ${name}};`);
  }
  body.push(`import.meta.mock.listeners.push(${
    () => {
      for (var k in mapping) {
        mapping[k] = import.meta.mock.namespace[k];
      }
    }
  });`);
  return body.join('\n');
}
