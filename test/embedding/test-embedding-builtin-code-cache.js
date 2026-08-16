'use strict';
// An embedder that bootstraps Node.js without a snapshot can generate a code
// cache for the builtins ahead of time (node::GenerateBuiltinCodeCache) and
// supply it at runtime (node::SetBuiltinCodeCache); the bootstrap and the
// per-context scripts then compile with that cache. Independently, it can ask
// Node.js not to serialize caches at runtime (kNoHarvestBuiltinCodeCache).
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const { spawnSyncAndAssert, spawnSyncAndExitWithoutError } = require('../common/child_process');
const fs = require('fs');

tmpdir.refresh();
const embedtest = common.resolveBuiltBinary('embedtest');
const cacheFile = tmpdir.resolve('builtins.codecache');

spawnSyncAndExitWithoutError(embedtest, ['--', '--builtin-code-cache-create', cacheFile], { cwd: tmpdir.path });
assert.ok(fs.statSync(cacheFile).size > 1024 * 1024);

function compileLog(args) {
  let log;
  spawnSyncAndAssert(
    embedtest, ['--', ...args, 'globalThis.ran = 40 + 2'],
    { cwd: tmpdir.path, env: { ...process.env, NODE_DEBUG_NATIVE: 'CODE_CACHE' } },
    { stderr(output) { log = output; return true; } });
  return log;
}

const without = compileLog([]);
assert.match(without, /Compiling internal\/bootstrap\/node without code cache/);

const withCache = compileLog(['--builtin-code-cache', cacheFile]);
assert.doesNotMatch(withCache, /without code cache/);
assert.match(withCache, /Code cache of internal\/bootstrap\/node \(BufferNotOwned\) is accepted/);
assert.match(withCache, /Code cache of internal\/per_context\/primordials \(BufferNotOwned\) is accepted/);

// Harvesting: by default a builtin compiled without a cache serializes one that
// worker threads then start from; with kNoHarvestBuiltinCodeCache they do not.
const workerScript = 'new (require("worker_threads").Worker)("", { eval: true })';
function workerCompileLog(args) {
  let log;
  spawnSyncAndAssert(
    embedtest, ['--', ...args, workerScript],
    { cwd: tmpdir.path, env: { ...process.env, NODE_DEBUG_NATIVE: 'CODE_CACHE' } },
    { stderr(output) { log = output; return true; } });
  const worker = log.slice(log.lastIndexOf('Compiling internal/bootstrap/realm'));
  assert.notStrictEqual(worker, log);
  return worker;
}
assert.match(workerCompileLog([]), /Code cache of internal\/bootstrap\/node \(\w+\) is accepted/);
assert.match(workerCompileLog(['--no-harvest-builtin-code-cache']),
             /Compiling internal\/bootstrap\/node without code cache/);
