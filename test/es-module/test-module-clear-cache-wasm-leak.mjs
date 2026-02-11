// Flags: --no-warnings

import { mustCall } from '../common/index.mjs';
import assert from 'node:assert';
import { clearCache, createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const { checkIfCollectableByCounting } = require('../common/gc');

const baseUrl = new URL('../fixtures/simple.wasm', import.meta.url);

const outer = 8;
const inner = 4;

const runIteration = mustCall(async (i) => {
  for (let j = 0; j < inner; j++) {
    const url = new URL(baseUrl);
    url.search = `?v=${i}-${j}`;
    const mod = await import(url.href);
    assert.strictEqual(mod.add(1, 2), 3);
    clearCache(url);
  }
  return inner;
}, outer);

checkIfCollectableByCounting(runIteration, WebAssembly.Instance, outer).then(mustCall());
