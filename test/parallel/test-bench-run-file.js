// Flags: --no-warnings
'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const { spawnSync } = require('child_process');
const { once } = require('events');
const path = require('path');
const { runFile } = require('node:bench');

const fixture = fixtures.path('bench-runner/run-file.cjs');

assert.throws(() => runFile(null), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => runFile('relative.cjs'), { code: 'ERR_INVALID_ARG_VALUE' });
assert.throws(() => runFile(fixture, null), { code: 'ERR_INVALID_ARG_TYPE' });
assert.throws(() => runFile(fixture, { execArgv: null }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
assert.throws(() => runFile(fixture, { execArgv: [1] }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
assert.throws(() => runFile(fixture, { execArgv: ['--bench'] }), {
  code: 'ERR_INVALID_ARG_VALUE',
});
assert.throws(() => runFile(fixture, { execArgv: [fixture] }), {
  code: 'ERR_INVALID_ARG_VALUE',
});
assert.throws(() => runFile(fixture, { execArgv: ['-e', '0'] }), {
  code: 'ERR_INVALID_ARG_VALUE',
});
assert.throws(() => runFile(fixture, { execArgv: ['--require'] }), {
  code: 'ERR_INVALID_ARG_VALUE',
});
assert.throws(() => runFile(fixture, { execArgv: ['--require='] }), {
  code: 'ERR_INVALID_ARG_VALUE',
});
assert.throws(() => runFile(fixture, { execArgv: [`-r=${fixture}`] }), {
  code: 'ERR_INVALID_ARG_VALUE',
});
assert.throws(() => runFile(fixture, { execArgv: ['--prof-process'] }), {
  code: 'ERR_INVALID_ARG_VALUE',
});
assert.throws(() => runFile(fixture, { execArgv: ['--prof_process'] }), {
  code: 'ERR_INVALID_ARG_VALUE',
});
assert.throws(() => runFile(fixture, {
  execArgv: ['--bench_name_pattern=run'],
}), { code: 'ERR_INVALID_ARG_VALUE' });
assert.throws(() => runFile(`${fixture}\0`), {
  code: 'ERR_INVALID_ARG_VALUE',
});
assert.throws(() => runFile(fixture, { env: null }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
assert.throws(() => runFile(fixture, { env: { INVALID: 1 } }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
assert.throws(() => runFile(fixture, { env: { INVALID: 'x\0' } }), {
  code: 'ERR_INVALID_ARG_VALUE',
});
assert.throws(() => runFile(fixture, { env: { NODE_CHANNEL_FD: 1 } }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
assert.throws(() => runFile(fixture, { signal: {} }), {
  code: 'ERR_INVALID_ARG_TYPE',
});
assert.throws(() => runFile(fixture, { signal: { aborted: false } }), {
  code: 'ERR_INVALID_ARG_TYPE',
});

async function testRunFile() {
  const execArgv = ['--expose-gc', '-r', 'fs'];
  const env = {
    __proto__: null,
    ...process.env,
    NODE_BENCH_CONTEXT: 'not-a-child',
    NODE_BENCH_RUN_FILE: 'original',
    NODE_CHANNEL_FD: '999',
    NODE_CHANNEL_SERIALIZATION_MODE: 'json',
  };
  const stream = runFile(fixture, { env, execArgv });
  execArgv.length = 0;
  env.NODE_BENCH_RUN_FILE = 'mutated';
  const records = await stream.toArray();
  const plan = records.find(({ type }) => type === 'bench:plan').data;
  const result = records.find(({ type }) => type === 'bench:complete').data;
  const summary = records.at(-1).data;

  assert.strictEqual(records.at(-1).type, 'bench:summary');
  assert.strictEqual(plan.params.context, 'child');
  assert.strictEqual(plan.params.exposed, true);
  assert.strictEqual(plan.params.value, 'original');
  assert.strictEqual(result.error, undefined);
  assert.strictEqual(result.samples.length, 1);
  assert.strictEqual(typeof result.samples[0].duration_ns, 'bigint');
  assert.notStrictEqual(result.samples[0].detail.pid, process.pid);
  assert(result.samples[0].detail.execArgv.includes('--expose-gc'));
  assert(result.samples[0].detail.execArgv.includes('-r'));
  assert.strictEqual(summary.success, true);
  assert.strictEqual(summary.file, fixture);
  assert.strictEqual(summary.entryFile, fixture);
  assert.strictEqual(summary.fileRunId, result.fileRunId);
  assert.strictEqual(summary.runId, result.runId);
  assert.strictEqual(records.filter(
    ({ type }) => type === 'bench:summary').length, 1);
}

async function testConcurrentCalls() {
  const [cjsRecords, esmRecords] = await Promise.all([
    runFile(fixtures.path('bench-runner/a.cjs')).toArray(),
    runFile(fixtures.path('bench-runner/b.mjs')).toArray(),
  ]);
  const cjsResult = cjsRecords.find(
    ({ type }) => type === 'bench:complete').data;
  const esmResult = esmRecords.find(
    ({ type }) => type === 'bench:complete').data;
  assert.strictEqual(cjsResult.name, 'alpha');
  assert.strictEqual(esmResult.name, 'beta');
  assert.notStrictEqual(cjsResult.params.pid, esmResult.params.pid);
  assert.notStrictEqual(cjsResult.runId, esmResult.runId);
}

async function testLoadFailure() {
  const missing = path.resolve(fixtures.fixturesDir, 'does-not-exist.cjs');
  const records = await runFile(missing).toArray();
  const diagnostics = records.filter(
    ({ type }) => type === 'bench:diagnostic');
  assert(diagnostics.some(
    ({ data }) => data.level === 'error' && /failed with exit code/.test(
      data.message)));
  assert.strictEqual(records.some(
    ({ type }) => type === 'bench:complete'), false);
  assert.strictEqual(records.at(-1).type, 'bench:summary');
  assert.strictEqual(records.at(-1).data.success, false);
}

async function testEvaluationFailure() {
  const records = await runFile(fixtures.path(
    'bench-runner/load-error-after-declaration.cjs')).toArray();
  assert(records.some(({ type, data }) =>
    type === 'bench:diagnostic' &&
    data.message === 'load failed after declaration'));
  const result = records.find(
    ({ type }) => type === 'bench:complete').data;
  assert.strictEqual(result.name, 'declared before load error');
  assert.strictEqual(result.error, undefined);
  assert.strictEqual(records.at(-1).data.counts.completed, 1);
  assert.strictEqual(records.at(-1).data.success, false);
}

async function testStructuredError() {
  const records = await runFile(
    fixtures.path('bench-runner/error.cjs')).toArray();
  const result = records.find(
    ({ type }) => type === 'bench:complete').data;
  assert.strictEqual(result.error.code, 'ERR_BENCHMARK_FIXTURE');
  assert.deepStrictEqual(result.error.cause, { value: 42n });
  assert.match(result.error.stack, /error\.cjs/);
  assert.strictEqual(records.at(-1).data.success, false);
}

async function testAbort() {
  const controller = new AbortController();
  const reason = new Error('cancel run file');
  const stream = runFile(
    fixtures.path('bench-runner/run-file-blocked.cjs'),
    { signal: controller.signal });
  stream.once('bench:start', common.mustCall(() => controller.abort(reason)));
  const records = await stream.toArray();
  const diagnostic = records.find(({ type, data }) =>
    type === 'bench:diagnostic' && data.error?.code === 'ABORT_ERR').data;
  assert.strictEqual(diagnostic.error.cause.message, reason.message);
  assert.strictEqual(records.some(
    ({ type }) => type === 'bench:complete'), false);
  assert.strictEqual(records.at(-1).type, 'bench:summary');
  assert.strictEqual(records.at(-1).data.success, false);
  assert.deepStrictEqual(records.at(-1).data.counts, {
    __proto__: null,
    completed: 0,
    failed: 0,
    skipped: 0,
    total: 1,
  });
}

async function testPartialAbort() {
  const controller = new AbortController();
  const stream = runFile(
    fixtures.path('bench-runner/run-file-partial.cjs'),
    { signal: controller.signal });
  stream.on('bench:start', ({ name }) => {
    if (name === 'aborted run file') controller.abort();
  });
  const records = await stream.toArray();
  const summary = records.at(-1).data;
  assert.strictEqual(records.filter(
    ({ type }) => type === 'bench:complete').length, 1);
  assert.deepStrictEqual(summary.counts, {
    __proto__: null,
    completed: 1,
    failed: 0,
    skipped: 0,
    total: 2,
  });
  assert.strictEqual(summary.success, false);
}

async function testPostSummaryAbort() {
  const controller = new AbortController();
  const stream = runFile(
    fixtures.path('bench-runner/run-file-lingering.cjs'),
    { signal: controller.signal });
  stream.on('bench:diagnostic', common.mustCall(({ message, stream }) => {
    if (stream === 'stdout' && /child disconnected/.test(message)) {
      controller.abort(new Error('stop lingering child'));
    }
  }, 2));
  const records = await stream.toArray();
  assert(records.some(({ type, data }) =>
    type === 'bench:diagnostic' && data.error?.code === 'ABORT_ERR'));
  assert.strictEqual(records.at(-1).type, 'bench:summary');
  assert.strictEqual(records.at(-1).data.counts.completed, 1);
  assert.strictEqual(records.at(-1).data.success, false);
}

async function testExitCode() {
  const records = await runFile(
    fixtures.path('bench-runner/exit-code.cjs')).toArray();
  assert(records.some(({ type, data }) =>
    type === 'bench:diagnostic' &&
    /set exit code/.test(data.message)));
  assert.strictEqual(records.at(-1).data.success, false);
}

async function testParentExitCode() {
  process.exitCode = 42;
  try {
    const records = await runFile(fixture).toArray();
    assert.strictEqual(records.at(-1).data.success, true);
  } finally {
    process.exitCode = undefined;
  }
}

async function testDefaultExecArgvSnapshot() {
  const stream = runFile(fixture);
  process.execArgv.push('--require=/does/not/exist.cjs');
  try {
    const records = await stream.toArray();
    assert.strictEqual(records.at(-1).data.success, true);
  } finally {
    process.execArgv.pop();
  }
}

async function testExecPathSnapshot() {
  const stream = runFile(fixture);
  const execPath = process.execPath;
  process.execPath = '/does/not/exist';
  try {
    const records = await stream.toArray();
    assert.strictEqual(records.at(-1).data.success, true);
  } finally {
    process.execPath = execPath;
  }
}

function testEvalParent() {
  const script = `
    require('node:bench').runFile(${JSON.stringify(fixture)})
      .on('bench:summary', (summary) => console.log(summary.success));
  `;
  const result = spawnSync(process.execPath, [
    '--no-warnings',
    '-e',
    script,
  ], { encoding: 'utf8' });
  assert.strictEqual(result.status, 0, result.stderr);
  assert.strictEqual(result.stdout, 'true\n');
}

async function testPreAborted() {
  const records = await runFile(fixture, {
    signal: AbortSignal.abort(new Error('already cancelled')),
  }).toArray();
  assert.strictEqual(records.some(({ type }) => type === 'bench:start'), false);
  assert.strictEqual(records.find(({ type }) =>
    type === 'bench:diagnostic').data.error.code, 'ABORT_ERR');
  assert.strictEqual(records.at(-1).type, 'bench:summary');
  assert.strictEqual(records.at(-1).data.success, false);
}

async function testDestroy() {
  const stream = runFile(
    fixtures.path('bench-runner/run-file-blocked.cjs'));
  stream.once('bench:start', common.mustCall(() => stream.destroy()));
  await once(stream, 'close');
  assert.strictEqual(stream.destroyed, true);
}

(async () => {
  await testRunFile();
  await testConcurrentCalls();
  await testLoadFailure();
  await testEvaluationFailure();
  await testStructuredError();
  await testAbort();
  await testPartialAbort();
  await testPostSummaryAbort();
  await testExitCode();
  await testParentExitCode();
  await testDefaultExecArgvSnapshot();
  await testExecPathSnapshot();
  await testPreAborted();
  await testDestroy();
  testEvalParent();
})().then(common.mustCall());
