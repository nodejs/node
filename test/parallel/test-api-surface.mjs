// Flags: --no-warnings
import '../common/index.mjs';
import { fileURL } from '../common/fixtures.mjs';

import { builtinModules } from 'node:module';
import { readFile } from 'node:fs/promises';
import { fail } from 'node:assert';

// TL;DR: If you intentionally changed the public API surface and now this test fails,
// please run: ./node tools/update-api-surface-fixture.mjs

// This file tests the exposed public API surface against the fixture.
// The exported properties are listed in `test/fixtures/apiSurface/`.
// You can regenerate it using `tools/update-api-surface-fixture.mjs`
// The main purpose of this test is to prevent unintentional increasing
// of API surface, when something undocumented gets exposed to userland.
// The secondary purpose is to help visualizing the observable changes,
// which works as reminder to update corresponding docs or tests.
// The third purpose is to catch errors such as getters that throw for
// any other reason than illegal invocation using prototype and not instance.

// Changes to this test should likely be applied to
// the tools/update-api-surface-fixture.mjs, and vice versa.


// Some properties can be present or absent depending on the environment
// Do not add them to the fixtures, do not test for their presence
const envSpecificProperties = {
  console: [
    // The `console._std*` are not documented and depend on current environment
    /^\{console\}(?:\.default|)\._(?:stderr|stdout)\./,
  ],
  module: [
    // The _cache and _pathCache are populated with local paths
    // Also, trying to access _cache.<common/index.js>.exports.then would throw
    /^\{module\}(?:\.Module|\.default|)\.(?:_cache|_pathCache)\./,
  ],
  process: [
    // The `common` adds listener to the `exit` event
    /^\{process\}(?:\.default|)\._events\.exit/,

    // The `--no-warnings` sets `process.noProcessWarnings` flag, ignore it
    /^\{process\}(?:\.default|)\.noProcessWarnings$/,

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

function isValid({ key, moduleName, path, propName, value, obj, fullPath }) {
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
  if (!obj || (typeof obj !== 'object' && typeof obj !== 'function')) return [];

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

const actualPaths = await Promise.all(builtinModules.map(async (moduleName) => {
  const module = await import(moduleName);
  return buildList(module, moduleName);
}));
const actual = new Set(actualPaths.flat());

const expectedPaths = await Promise.all(builtinModules.map(async (moduleName) => {
  const res = (await readFile(fileURL(`apiSurface/${moduleName.replace(/(?::|\/)/g, '-')}`), 'utf8'));
  return res.split('\n').filter(Boolean);
}));
const expected = new Set(expectedPaths.flat());

const added = actual.difference(expected);
const removed = expected.difference(actual);

// Not using deepStrictEqual because it would produce long and less informative error
if (added.size || removed.size) {
  const addedMessage = added.size ? `\n\nAdded ${added.size} properties:\n${[...added].join('\n')}` : '';
  const removedMessage = removed.size ? `\n\nRemoved ${removed.size} properties:\n${[...removed].join('\n')}` : '';
  const message = `
Observed API surface is different from the lists in test/fixtures/apiSurface/
${addedMessage}${removedMessage}

If all these changes are intentional, please run:
./node tools/update-api-surface-fixture.mjs
`;

  // Not using fail(message) because the message might get truncated
  console.error(message);

  fail(`Found ${added.size + removed.size} changes(s) to public API, see above`);
}
