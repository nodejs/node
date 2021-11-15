import { receiveMessageOnPort } from 'node:worker_threads';
const mockedModuleExports = new Map();
let currentMockVersion = 0;

/**
 * FIXME: this is a hack to workaround loaders being
 * single threaded for now
 */
function doDrainPort() {
  let msg;
  while (msg = receiveMessageOnPort(preloadPort)) {
    onPreloadPortMessage(msg.message);
  }
}
function onPreloadPortMessage({
  mockVersion, resolved, exports
}) {
  currentMockVersion = mockVersion;
  mockedModuleExports.set(resolved, exports);
}
let preloadPort;
export function globalPreload({port}) {
  preloadPort = port;
  port.on('message', onPreloadPortMessage);
  port.unref();
  const insideAppContext = (getBuiltin, port, setImportMetaCallback) => {
    let mockedModules = new Map();
    let mockVersion = 0;
    const doMock = (resolved, replacementProperties) => {
      let exports = Object.keys(replacementProperties);
      let namespace = Object.create(null);
      let listeners = [];
      for (const name of exports) {
        let currentValue = replacementProperties[name];
        Object.defineProperty(namespace, name, {
          enumerable: true,
          get() {
            return currentValue;
          },
          set(v) {
            currentValue = v;
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
      port.postMessage({ mockVersion, resolved, exports });
      return namespace;
    }
    setImportMetaCallback((meta, context, parent) => {
      if (context.url === 'node:mock') {
        meta.doMock = doMock;
        return;
      }
      if (context.url.startsWith('mock:')) {
        let [proto, version, encodedTargetURL] = context.url.split(':');
        let decodedTargetURL = decodeURIComponent(encodedTargetURL);
        if (mockedModules.has(decodedTargetURL)) {
          meta.mock = mockedModules.get(decodedTargetURL);
          return;
        }
      }
      parent(meta, context);
    });
  };
  return `(${insideAppContext})(getBuiltin, port, setImportMetaCallback)`
}


// rewrites node: loading to mock: so that it can be intercepted
export function resolve(specifier, context, defaultResolve) {
  if (specifier === 'node:mock') {
    return {
      url: specifier
    };
  }
  doDrainPort();
  const def = defaultResolve(specifier, context);
  if (context.parentURL?.startsWith('mock:')) {
    // do nothing, let it get the "real" module
  } else if (mockedModuleExports.has(def.url)) {
    return {
      url: `mock:${currentMockVersion}:${encodeURIComponent(def.url)}`
    };
  };
  return {
    url: `${def.url}`
  };
}

export function load(url, context, defaultLoad) {
  doDrainPort();
  if (url === 'node:mock') {
    return {
      source: 'export default import.meta.doMock',
      format: 'module'
    };
  }
  if (url.startsWith('mock:')) {
    let [proto, version, encodedTargetURL] = url.split(':');
    let ret = generateModule(mockedModuleExports.get(
      decodeURIComponent(encodedTargetURL)
    ));
    return {
      source: ret,
      format: 'module'
    };
  }
  return defaultLoad(url, context);
}

function generateModule(exports) {
  let body = 'export {};let mapping = {__proto__: null};'
  for (const [i, name] of Object.entries(exports)) {
    let key = JSON.stringify(name);
    body += `var _${i} = import.meta.mock.namespace[${key}];`
    body += `Object.defineProperty(mapping, ${key}, {enumerable: true,set(v) {_${i} = v;}});`
    body += `export {_${i} as ${name}};`;
  }
  body += `import.meta.mock.listeners.push(${
    () => {
      for (var k in mapping) {
        mapping[k] = import.meta.mock.namespace[k];
      }
    }
  });`
  return body;
}
