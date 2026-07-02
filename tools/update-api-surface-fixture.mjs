/*
 * This script updates files in `test/fixtures/apiSurface/`,
 * used by the `test/parallel/test-api-surface.mjs` test.
 * If you've made changes that intentionally add or remove something,
 * run this script and check that all changes it makes are intentional.
 * You can view fixture changes using `git diff test/fixtures/apiSurface`
 * Manually editing the fixture is also okay.
 * Changes to this script usually should be mirrored in
 * the `test/parallel/test-api-surface.mjs` test.
 */

import { builtinModules } from 'node:module';
import { writeFile } from 'node:fs/promises';

// Some properties can be present or absent depending on the environment
// Do not add them to the fixtures, do not test for their presence
const envSpecificProperties = {
  console: [
    // The `console._std*` are not documented and depend on current environment
    /^\{console\}(?:\.default|)\._(?:stderr|stdout)\./,
  ],
  module: [
    // The _cache and _pathCache are populated with local paths
    /^\{module\}(?:\.Module|\.default|)\.(?:_cache|_pathCache)\./,
  ],
  process: [
    // These depend on current environment
    /^\{process\}(?:\.default|)\.env\./,
    /^\{process\}(?:\.default|)\.config\.variables\./,
    /^\{process\}(?:\.default|)\.(?:stderr|stdin|stdout)\./,

    // The `process._events` is not documented and might have env-specific listeners
    /^\{process\}(?:\.default|)\._events\./,
  ],
};

// Some properties are platform-specific.
// TODO(LiviaMedeiros): consider making this more strict by creating additional lists
// with all per-platform entries. For now, we're just skipping them.
const platformSpecificProperties = {
  'constants': [
    /^\{constants\}(?:\.default|)\./,
  ],
  'fs': [
    /^\{fs\}(?:\.default|)(?:\.promises|)\.constants\./,

    // The lchmod and lchmodSync are deprecated and available only on macOS
    /^\{fs\}(?:\.default|)\.(?:lchmod|lchmodSync)/,
  ],
  'fs/promises': [
    /^\{fs\/promises\}(?:\.default|)\.constants\./,
  ],
  'os': [
    /^\{os\}(?:\.default|)\.constants\./,
  ],
};

function isValid({ moduleName, propName, obj, fullPath }) {
  // Skip array element indices: we only want methods and properties, not each index
  if (Array.isArray(obj) && /^\d+$/.test(propName))
    return false;

  if (envSpecificProperties[moduleName])
    for (const skipRegex of envSpecificProperties[moduleName])
      if (skipRegex.test(fullPath))
        return false;

  if (platformSpecificProperties[moduleName])
    for (const skipRegex of platformSpecificProperties[moduleName])
      if (skipRegex.test(fullPath))
        return false;

  return true;
}

async function buildList(obj, moduleName, path = [], visited = new WeakMap()) {
  if (!obj || (typeof obj !== 'object' && typeof obj !== 'function'))
    return [];

  if (path.length > 16) {
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
    visited.set(obj, [ path ]);
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

const paths = await Promise.all(builtinModules.map(async (moduleName) => [
  moduleName.replace(/(?::|\/)/g, '-'),
  await buildList(await import(moduleName), moduleName),
]));

await Promise.all(paths.map(([ safeModuleName, propertyList ]) => {
  const fixtureURL = new URL(`../test/fixtures/apiSurface/${safeModuleName}`, import.meta.url);
  return writeFile(fixtureURL, `${propertyList.sort().join('\n')}\n`);
}));
