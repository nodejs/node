// Flags: --no-warnings
import '../common/index.mjs';
import { builtinModules } from 'node:module';

// This test recursively checks all properties on builtin modules and ensures that accessing them
// does not throw. The only valid reason for property accessor to throw is "invalid this" error
// when property must be not accessible directly on the prototype.

// Normally we don't have properties nested too deep
const MAX_NESTING_DEPTH = 16;

// Some properties can be present or absent depending on the environment
const knownExceptions = {
  module: [
    // The _cache and _pathCache are populated with local paths
    // Also, trying to access _cache.<common/index.js>.exports.then would throw
    /^\{module\}(?:\.Module|\.default|)\.(?:_cache|_pathCache)\./,
  ],
};

function isValid({ key, moduleName, path, propName, value, obj, fullPath }) {
  if (knownExceptions[moduleName])
    for (const skipRegex of knownExceptions[moduleName])
      if (skipRegex.test(fullPath))
        return false;

  return true;
}

function shouldSkipModule(moduleName) {
  switch (moduleName) {
    case 'inspector':
    case 'inspector/promises':
      if (!process.features.inspector) {
        return true;
      }
      break;
  }
  return false;
}

async function buildList(obj, moduleName, path = [], visited = new WeakMap()) {
  if (!obj || (typeof obj !== 'object' && typeof obj !== 'function')) return [];

  if (path.length > MAX_NESTING_DEPTH) {
    throw new Error(`Too deeply nested property in ${moduleName}: ${path.join('.')}`);
  }

  // Exclude circular references
  // Don't use plain Set, because same object can be exposed under different paths
  if (visited.has(obj)) {
    const visitedPaths = visited.get(obj);
    if (visitedPaths.some((prev) => prev.length <= path.length && prev.every((seg, i) => seg === path[i]))) {
      return [];
    }
    visitedPaths.push(path);
  } else {
    visited.set(obj, [path]);
  }

  const paths = [];
  const deeperCalls = [];

  for (const key of Reflect.ownKeys(obj)) {
    const propName = typeof key === 'symbol' ? `[${key.description}]` : key;
    const fullPath = `{${moduleName}}.${path.join('.')}${path.length ? '.' : ''}${propName}`;

    if (!isValid({ key, moduleName, path, propName, obj, fullPath })) {
      continue;
    }

    let value;
    try {
      value = await obj[key];
    } catch (cause) {
      // Accessing some properties directly on the prototype may throw or reject
      // Throw informative errors if access failed anywhere/anyhow else
      if (cause.name !== 'TypeError') {
        throw new Error(`Access to ${fullPath} failed with name=${cause.name}`, { cause });
      }
      if (cause.code !== 'ERR_INVALID_THIS' && cause.code !== undefined) {
        throw new Error(`Access to ${fullPath} failed with code=${cause.code}`, { cause });
      }
      if (path.at(-1) !== 'prototype') {
        throw new Error(`Access to ${fullPath} failed but it's not on prototype`, { cause });
      }
    }
    paths.push(fullPath);

    if ((typeof value === 'object' || typeof value === 'function') && value !== obj) {
      deeperCalls.push(buildList(value, moduleName, [...path, propName], visited));
    }
  }

  return [...paths, ...(await Promise.all(deeperCalls)).flat()];
}

let total = 0;

await Promise.all(builtinModules.map(async (moduleName) => {
  if (shouldSkipModule(moduleName)) {
    return;
  }
  const module = await import(moduleName);
  const { length } = await buildList(module, moduleName);
  total += length;
}));

console.log(`Checked ${total} properties across ${builtinModules.length} modules.`);
