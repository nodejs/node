'use strict';

// The ES module loader of green threads. Modules come from the bundle that
// the host passed, or from the host's load() hook. Specifiers that are URLs,
// or relative to the importing module's URL, are resolved as URLs, others
// with the host's resolve() hook. A module's whole graph is fetched before
// it is linked, since V8 links modules synchronously.

const {
  ArrayPrototypePush,
  JSONParse,
  ObjectKeys,
  PromisePrototypeThen,
  PromiseWithResolvers,
  SafeMap,
  SafeSet,
  StringPrototypeStartsWith,
} = primordials;

const {
  compileModule,
  createJsonModule,
  evaluateModule,
  instantiateModule,
  linkModule,
  moduleNamespace,
  moduleRequests,
  resolveUrl,
} = internalBinding('green_thread_runtime');
const { modules: errors } = require('internal/green_threads/runtime/errors');

// URL => { source, format } or source.
let bundle = { __proto__: null };
// A stub of the host's loader, with resolve() and load() methods, if the
// host has hooks.
let hostLoader = null;
let hasResolveHook = false;
let hasLoadHook = false;

// Like Promise.all(), without using what user code can change.
function all(promises) {
  const { promise, resolve, reject } = PromiseWithResolvers();
  const results = [];
  let remaining = promises.length;
  if (remaining === 0) resolve(results);
  for (let i = 0; i < promises.length; i++) {
    PromisePrototypeThen(promises[i], (value) => {
      results[i] = value;
      if (--remaining === 0) resolve(results);
    }, reject);
  }
  return promise;
}

// `${type}:${url}` => promise of the ID of the module.
const records = new SafeMap();
// ID => promise of the IDs of the modules that its requests resolve to,
// once it's linked.
const dependencies = new SafeMap();
// ID => URL.
const urls = new SafeMap();

function configure({ modules, loader, hasResolve, hasLoad }) {
  if (modules != null) bundle = modules;
  if (loader != null) {
    hostLoader = loader;
    hasResolveHook = !!hasResolve;
    hasLoadHook = !!hasLoad;
  }
}

function isRelativeSpecifier(specifier) {
  return StringPrototypeStartsWith(specifier, './') ||
    StringPrototypeStartsWith(specifier, '../') ||
    StringPrototypeStartsWith(specifier, '/');
}

function checkScheme(url) {
  if (StringPrototypeStartsWith(url, 'node:')) {
    throw errors.unsupportedScheme(url);
  }
  return url;
}

async function resolve(specifier, referrer, attributes) {
  if (isRelativeSpecifier(specifier)) {
    const url = referrer === undefined ? undefined :
      resolveUrl(specifier, referrer);
    if (url !== undefined) return checkScheme(url);
  } else {
    const url = resolveUrl(specifier);
    if (url !== undefined) return checkScheme(url);
  }
  if (hasResolveHook) {
    const result = await hostLoader.resolve(specifier,
                                            { referrer, attributes });
    if (result !== undefined) return checkScheme(result.url);
  }
  throw errors.notFound(specifier, referrer);
}

function attributeType(attributes) {
  const keys = ObjectKeys(attributes);
  for (let i = 0; i < keys.length; i++) {
    if (keys[i] !== 'type') {
      throw errors.attributeUnsupported(keys[i], attributes[keys[i]]);
    }
  }
  const { type } = attributes;
  if (type !== undefined && type !== 'json') {
    throw errors.attributeUnsupported('type', type);
  }
  return type;
}

// Resolves with the ID of the module at `url`, which is compiled once for
// each type.
function getRecord(url, attributes) {
  const type = attributeType(attributes);
  const key = `${type ?? 'module'}:${url}`;
  let record = records.get(key);
  if (record === undefined) {
    record = fetchRecord(url, type);
    records.set(key, record);
  }
  return record;
}

async function fetchRecord(url, type) {
  let entry = bundle[url];
  if (entry === undefined && hasLoadHook) {
    entry = await hostLoader.load(url, {
      attributes: type === undefined ? {} : { type },
    });
  }
  if (entry === undefined) throw errors.notFound(url);
  // The host checked it.
  const { source, format = 'module' } =
    typeof entry === 'string' ? { source: entry } : entry;
  let id;
  if (format === 'json') {
    if (type !== 'json') throw errors.attributeMissing(url, 'type', 'json');
    id = createJsonModule(url, JSONParse(source));
  } else if (format === 'module') {
    if (type === 'json') throw errors.attributeIncompatible(url, 'json');
    id = compileModule(source, url);
  } else {
    throw errors.unknownFormat(format, url);
  }
  urls.set(id, url);
  return id;
}

function getDependencies(id) {
  const url = urls.get(id);
  let result = dependencies.get(id);
  if (result === undefined) {
    result = (async () => {
      const requests = moduleRequests(id);
      const ids = [];
      for (let i = 0; i < requests.length; i++) {
        const { 0: specifier, 1: attributes } = requests[i];
        ArrayPrototypePush(ids, (async () => {
          return getRecord(await resolve(specifier, url, attributes),
                           attributes);
        })());
      }
      const resolved = await all(ids);
      linkModule(id, resolved);
      return resolved;
    })();
    dependencies.set(id, result);
  }
  return result;
}

// Fetches and links the graph of the module `id`.
async function loadGraph(id) {
  const visited = new SafeSet();
  const visit = async (id) => {
    if (visited.has(id)) return;
    visited.add(id);
    const ids = await getDependencies(id);
    const visits = [];
    for (let i = 0; i < ids.length; i++) ArrayPrototypePush(visits, visit(ids[i]));
    await all(visits);
  };
  await visit(id);
}

async function run(id) {
  await loadGraph(id);
  instantiateModule(id);
  await evaluateModule(id);
  return moduleNamespace(id);
}

/**
 * Imports the module `specifier` for import(), from a module or script at
 * `referrer`, if known.
 * @returns {Promise<object>} The namespace of the module.
 */
async function importModule(specifier, referrer, attributes) {
  const url = await resolve(specifier, referrer, attributes);
  return run(await getRecord(url, attributes));
}

/**
 * Runs the main module of a green thread, at `url`.
 * @returns {Promise<object>} The namespace of the module.
 */
function runMain(source, url) {
  const id = compileModule(source, url);
  urls.set(id, url);
  records.set(`module:${url}`, id);
  return run(id);
}

module.exports = {
  configure,
  importModule,
  runMain,
};
