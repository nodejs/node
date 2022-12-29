// This is expected to be used by test-esm-loader-hooks.mjs via:
// node --loader ./test/fixtures/es-module-loaders/hooks-input.mjs ./test/fixtures/es-modules/json-modules.mjs

import assert from 'assert';
import { write } from 'fs';
import { readFile } from 'fs/promises';
import { fileURLToPath } from 'url';


let resolveCalls = 0;
let loadCalls = 0;

export async function resolve(specifier, context, next) {
  resolveCalls++;
  let url;

  if (resolveCalls === 1) {
    url = new URL(specifier).href;
    assert.match(specifier, /json-modules\.mjs$/);
    assert.deepStrictEqual(context.parentURL, undefined);
    assert.deepStrictEqual(context.importAssertions, {
      __proto__: null,
    });
  } else if (resolveCalls === 2) {
    url = new URL(specifier, context.parentURL).href;
    assert.match(specifier, /experimental\.json$/);
    assert.match(context.parentURL, /json-modules\.mjs$/);
    assert.deepStrictEqual(context.importAssertions, {
      __proto__: null,
      type: 'json',
    });
  }

  // Ensure `context` has all and only the properties it's supposed to
  assert.deepStrictEqual(Object.keys(context), [
    'conditions',
    'importAssertions',
    'parentURL',
  ]);
  assert.ok(Array.isArray(context.conditions));
  assert.deepStrictEqual(typeof next, 'function');

  const returnValue = {
    url,
    format: 'test',
    shortCircuit: true,
  }

  await new Promise(resolve => write(1, `${JSON.stringify(returnValue)}\n`, resolve)); // For the test to read

  return returnValue;
}

export async function load(url, context, next) {
  loadCalls++;
  const source = await readFile(fileURLToPath(url));
  let format;

  if (loadCalls === 1) {
    assert.match(url, /json-modules\.mjs$/);
    assert.deepStrictEqual(context.importAssertions, {
      __proto__: null,
    });
    format = 'module';
  } else if (loadCalls === 2) {
    assert.match(url, /experimental\.json$/);
    assert.deepStrictEqual(context.importAssertions, {
      __proto__: null,
      type: 'json',
    });
    format = 'json';
  }

  assert.ok(new URL(url));
  // Ensure `context` has all and only the properties it's supposed to
  assert.deepStrictEqual(Object.keys(context), [
    'format',
    'importAssertions',
  ]);
  assert.deepStrictEqual(context.format, 'test');
  assert.deepStrictEqual(typeof next, 'function');

  const returnValue = {
    source,
    format,
    shortCircuit: true,
  };

  await new Promise(resolve => write(1, `${JSON.stringify(returnValue)}\n`, resolve)); // For the test to read

  return returnValue;
}
