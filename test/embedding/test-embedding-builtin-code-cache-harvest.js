'use strict';
// By default a builtin compiled without a code cache serializes one that worker
// threads then start from; with kNoHarvestBuiltinCodeCache it does not.
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const { spawnSyncAndAssert } = require('../common/child_process');

tmpdir.refresh();
const embedtest = common.resolveBuiltBinary('embedtest');
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
