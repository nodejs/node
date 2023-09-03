const mockedModuleExports = new Map();
let currentMockVersion = 0;

// This loader enables code running on the application thread to
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
// import mock from 'test-esm-loader-mock'; // See test-esm-loader-mock.mjs
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


export async function initialize({ port }) {
  port.on('message', ({ mockVersion, resolved, exports }) => {
    currentMockVersion = mockVersion;
    mockedModuleExports.set(resolved, exports);
  });
}

// Rewrites node: loading to mock-facade: so that it can be intercepted
export async function resolve(specifier, context, defaultResolve) {
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
  /**
   * Mocked fake module, not going to be handled in default way so it
   * generates the source text, then short circuits
   */
  if (url.startsWith('mock-facade:')) {
    let [_proto, _version, encodedTargetURL] = url.split(':');
    let source = generateModule(encodedTargetURL);
    return {
      shortCircuit: true,
      source,
      format: 'module'
    };
  }
  return defaultLoad(url, context);
}

const mainImportURL = new URL('./mock.mjs', import.meta.url);
/**
 * Generate the source code for a mocked module.
 * @param {string} encodedTargetURL the module being mocked
 * @returns {string}
 */
function generateModule(encodedTargetURL) {
  const exports = mockedModuleExports.get(
    decodeURIComponent(encodedTargetURL)
  );
  let body = [
    `import { mockedModules } from '${mainImportURL}';`,
    'export {};',
    'let mapping = {__proto__: null};'
  ];
  for (const [i, name] of Object.entries(exports)) {
    let key = JSON.stringify(name);
    body.push(`import.meta.mock = mockedModules.get(${JSON.stringify(encodedTargetURL)});`);
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
