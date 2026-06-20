'use strict';
// Tests for the test_runner `flaky` feature (retry-until-pass).

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const assert = require('node:assert');
const { writeFileSync } = require('node:fs');
const { test, run } = require('node:test');

tmpdir.refresh();

let fixtureSeq = 0;
function writeFixture(source) {
  const file = tmpdir.resolve(`flaky-fixture-${fixtureSeq++}.js`);
  writeFileSync(file, source);
  return file;
}

async function drainRun(file, runOptions, subscribe) {
  const stream = run({ files: [file], ...runOptions });
  subscribe(stream);
  // eslint-disable-next-line no-unused-vars
  for await (const _ of stream);
}

// Collect the run() event stream for a fixture file, bucketed by test name.
async function collectEvents(file, runOptions = {}) {
  const events = { pass: [], fail: [], diagnostic: [] };
  await drainRun(file, runOptions, (stream) => {
    stream.on('test:pass', (data) => events.pass.push(data));
    stream.on('test:fail', (data) => events.fail.push(data));
    stream.on('test:diagnostic', (data) => events.diagnostic.push(data));
    stream.on('test:stderr', common.mustNotCall());
  });
  return events;
}

function byName(list, name) {
  return list.filter((d) => d.name === name);
}

async function tap(file) {
  const { code, stdout, stderr } = await common.spawnPromisified(
    process.execPath,
    ['--test-reporter=tap', file],
  );
  return { code, stdout, stderr };
}

// Run a fixture through the run() API under the default 'process' isolation and
// return the aggregated parent summary counters. Exercises the cross-process
// path where the parent re-counts the child's serialized events (runner.js).
async function runSummaryCounts(file, runOptions = {}) {
  let counts;
  await drainRun(file, runOptions, (stream) => {
    stream.on('test:summary', (data) => { counts = data.counts; });
  });
  return counts;
}

function validate(flaky) {
  return test(`flaky-validate-${String(flaky)}`, { flaky }, () => {});
}

test('flaky validation: wrong types throw ERR_INVALID_ARG_TYPE', () => {
  for (const flaky of [Symbol(), {}, [], () => {}, 1n, '1', '5']) {
    assert.throws(() => validate(flaky), { code: 'ERR_INVALID_ARG_TYPE' });
  }
});

test('flaky validation: out-of-range values throw ERR_OUT_OF_RANGE', () => {
  for (const flaky of [0, -1, -20, 1.5, 2.0001, NaN, Infinity, -Infinity]) {
    assert.throws(() => validate(flaky), { code: 'ERR_OUT_OF_RANGE' });
  }
});

test('flaky validation: true and positive integers are accepted', async () => {
  for (const flaky of [null, undefined, true, false, 1, 2, 20, 100, 2 ** 20]) {
    await validate(flaky); // Must not throw / reject for valid values.
  }
});

test('flaky: passes after retries, retryCount on pass, silent intermediates', async () => {
  const file = fixtures.path('test-runner/flaky/eventually-passes.js');
  const events = await collectEvents(file);

  const passes = byName(events.pass, 'passes on third attempt');
  const fails = byName(events.fail, 'passes on third attempt');

  assert.strictEqual(passes.length, 1, `expected 1 pass, got ${passes.length}`);
  assert.strictEqual(fails.length, 0, `expected 0 fail events, got ${fails.length}`);
  assert.strictEqual(passes[0].retryCount, 2);
  const messages = events.diagnostic.map((d) => d.message);
  assert.ok(messages.includes('attempt-3'), `missing final diagnostic; got ${messages}`);
  assert.ok(!messages.includes('attempt-1'), `leaked intermediate diagnostic attempt-1: ${messages}`);
  assert.ok(!messages.includes('attempt-2'), `leaked intermediate diagnostic attempt-2: ${messages}`);

  const { code, stdout } = await tap(file);
  assert.strictEqual(code, 0);
  assert.match(stdout, /ok 1 - passes on third attempt # FLAKY 2 re-tries/);
  assert.match(stdout, /# pass 1$/m);
  assert.match(stdout, /# fail 0$/m);
  assert.match(stdout, /# flaky 1$/m);
});

test('flaky: exhaustion => fail with retryCount on fail', async () => {
  const file = fixtures.path('test-runner/flaky/exhausts.js');
  const events = await collectEvents(file);

  const fails = byName(events.fail, 'always fails');
  const passes = byName(events.pass, 'always fails');
  assert.strictEqual(passes.length, 0);
  assert.strictEqual(fails.length, 1, `expected 1 fail, got ${fails.length}`);
  assert.strictEqual(fails[0].retryCount, 3);

  const { code, stdout } = await tap(file);
  assert.strictEqual(code, 1);
  assert.match(stdout, /not ok 1 - always fails # FLAKY failed after 3 re-tries/);
  assert.match(stdout, /# fail 1$/m);
  assert.match(stdout, /# flaky 1$/m);
});

test('flaky: non-throwing failure (t.fail/plan) is retried', async () => {
  const file = fixtures.path('test-runner/flaky/non-throw-retry.js');
  const events = await collectEvents(file);
  const passes = byName(events.pass, 'non-throw failure retried');
  const fails = byName(events.fail, 'non-throw failure retried');
  assert.strictEqual(fails.length, 0);
  assert.strictEqual(passes.length, 1);
  assert.strictEqual(passes[0].retryCount, 1);
});

// The fixture runs as a spawned child and can't call common.platformTimeout
// itself, so the budget is computed here and interpolated into the source. The
// base is deliberately generous: attempt 2 is a no-op that must COMPLETE within
// a fresh timeout window, and that bound does NOT stretch under load, so a tiny
// budget could spuriously time out the no-op and break the retryCount === 1
// equality below. platformTimeout() additionally covers known-slow platforms.
const RETRY_TIMEOUT = common.platformTimeout(1000);
// Attempt 1's delay must stay well above the timeout even after scaling; bind it
// to t.signal so attempt 1 ends at ~timeout with no dangling timer.
const RETRY_DELAY = RETRY_TIMEOUT * 10;
const timeoutRetrySrc = `
'use strict';
const { test } = require('node:test');
const { setTimeout } = require('node:timers/promises');
let attempts = 0;
test('slow first attempt then fast', { flaky: 3, timeout: ${RETRY_TIMEOUT} }, async (t) => {
  attempts++;
  if (attempts < 2) {
    await setTimeout(${RETRY_DELAY}, undefined, { signal: t.signal }).catch(() => {});
  }
});
`;

test('flaky: timeout on first attempt is retried then passes', async () => {
  const file = writeFixture(timeoutRetrySrc);
  const events = await collectEvents(file);
  const passes = byName(events.pass, 'slow first attempt then fast');
  const fails = byName(events.fail, 'slow first attempt then fast');
  assert.strictEqual(fails.length, 0, `timeout should be retried, got fails=${fails.length}`);
  assert.strictEqual(passes.length, 1);
  assert.strictEqual(passes[0].retryCount, 1);
});

test('flaky: exhausted timeout is FAIL, non-flaky timeout stays cancelled', async () => {
  const file = fixtures.path('test-runner/flaky/timeout-exhaust.js');
  await collectEvents(file);
  const { code, stdout } = await tap(file);
  assert.strictEqual(code, 1);
  assert.match(stdout, /not ok \d+ - flaky timeout exhausts => fail # FLAKY failed after 2 re-tries/);
  assert.match(stdout, /# cancelled 1$/m);
  assert.match(stdout, /# flaky 1$/m);
});

test('flaky: external abort stops retrying, body runs once', async () => {
  const file = fixtures.path('test-runner/flaky/external-abort.js');
  const { code, stdout, stderr, state: runs } = await spawnFlaky(file, 'abort-state.txt');
  assert.strictEqual(stderr, '');
  assert.strictEqual(code, 1);
  assert.strictEqual(runs.length, 1, `body ran ${runs.length} times, expected 1`);
  assert.match(stdout, /# flaky 1$/m);
  assert.doesNotMatch(stdout, /FLAKY failed after \d+ re-tries/);
});

test('flaky: beforeEach/afterEach re-run each attempt, before/after once', async () => {
  const file = fixtures.path('test-runner/flaky/hooks.js');
  const { code, stderr, state } = await spawnFlaky(file, 'hooks-state.json');
  assert.strictEqual(stderr, '');
  assert.strictEqual(code, 0);
  const counts = JSON.parse(state);
  assert.strictEqual(counts.body, 3, `body should run once per attempt, got ${counts.body}`);
  assert.strictEqual(counts.beforeEach, 3, `beforeEach per attempt, got ${counts.beforeEach}`);
  assert.strictEqual(counts.afterEach, 3, `afterEach per attempt, got ${counts.afterEach}`);
  assert.strictEqual(counts.before, 1, `before once, got ${counts.before}`);
  assert.strictEqual(counts.after, 1, `after once, got ${counts.after}`);
});

test('flaky: suite inheritance, child override, child opt-out', async () => {
  const file = fixtures.path('test-runner/flaky/inheritance.js');
  const events = await collectEvents(file);

  const inherit = byName(events.pass, 'inherits suite flaky');
  assert.strictEqual(inherit.length, 1);
  assert.strictEqual(inherit[0].retryCount, 2);

  const override = byName(events.pass, 'overrides with own flaky');
  assert.strictEqual(override.length, 1);
  assert.strictEqual(override[0].retryCount, 1);

  const disabledFail = byName(events.fail, 'opted out via flaky:false');
  assert.strictEqual(disabledFail.length, 1);
  assert.ok(!(disabledFail[0].retryCount > 0),
            `opted-out test should not retry, got retryCount=${disabledFail[0].retryCount}`);

  const { stdout } = await tap(file);
  assert.match(stdout, /# flaky 2$/m);
});

test('flaky: summary counter counts marked-flaky incl. pass-first-try', async () => {
  const file = fixtures.path('test-runner/flaky/counter.js');
  const { code, stdout } = await tap(file);
  assert.strictEqual(code, 0);
  assert.match(stdout, /# pass 3$/m);
  assert.match(stdout, /# flaky 2$/m);
  assert.doesNotMatch(stdout, /flaky but passes first try # FLAKY/);
});

test('flaky:true defaults to 20 retries', async () => {
  const file = fixtures.path('test-runner/flaky/default-twenty.js');
  const events = await collectEvents(file);
  const passes = byName(events.pass, 'passes on attempt 21');
  const fails = byName(events.fail, 'passes on attempt 21');
  assert.strictEqual(fails.length, 0);
  assert.strictEqual(passes.length, 1);
  assert.strictEqual(passes[0].retryCount, 20);
});

test('flaky:true exhausts at 20 retries when more are needed', async () => {
  const file = fixtures.path('test-runner/flaky/default-twenty-exhaust.js');
  const events = await collectEvents(file);
  const fails = byName(events.fail, 'needs 22 attempts');
  assert.strictEqual(fails.length, 1);
  assert.strictEqual(fails[0].retryCount, 20);
});

test('flaky: expectFailure composes => no retry', async () => {
  const file = fixtures.path('test-runner/flaky/expect-failure.js');
  const { code, stdout, stderr, state: runs } = await spawnFlaky(file, 'xfail-state.txt');
  assert.strictEqual(stderr, '');
  assert.strictEqual(code, 0);
  assert.strictEqual(runs.length, 1, `body ran ${runs.length} times, expected 1 (no retry)`);
  assert.match(stdout, /# flaky 1$/m);
  assert.doesNotMatch(stdout, /FLAKY \d+ re-tries/);
});

test('flaky: all API surface forms retry identically (forms 1-5 + aliases)', async () => {
  const file = fixtures.path('test-runner/flaky/api-surface.js');
  const events = await collectEvents(file);

  // Each name maps to one proposal API form; all must behave identically.
  const names = [
    'member inherits describe.flaky', // Form 1: describe.flaky suite.
    'member inherits options flaky',  // Form 3: describe('name', { flaky: true }).
    'member inherits suite.flaky',    // Alias: suite.flaky suite.
    'it.flaky case',                  // Form 2: it.flaky test-case.
    'it options boolean',             // Form 4: it('name', { flaky: true }).
    'it options integer',             // Form 5: it('name', { flaky: 100 }).
    'test.flaky case',                // Alias: test.flaky.
    'test options integer',           // Alias: test('name', { flaky: N }).
  ];
  for (const name of names) {
    const passes = byName(events.pass, name);
    const fails = byName(events.fail, name);
    assert.strictEqual(fails.length, 0, `${name}: unexpected fail events`);
    assert.strictEqual(passes.length, 1, `${name}: expected exactly one pass`);
    assert.strictEqual(passes[0].retryCount, 1, `${name}: expected retryCount 1`);
  }
});

test('flaky: run() parent counts flaky and promotes exhausted timeout to fail', async () => {
  const file = fixtures.path('test-runner/flaky/run-parent-summary.js');
  const counts = await runSummaryCounts(file);
  assert.strictEqual(counts.failed, 1, `failed=${counts.failed}`);
  assert.strictEqual(counts.cancelled, 1, `cancelled=${counts.cancelled}`);
  assert.strictEqual(counts.flaky, 2, `flaky=${counts.flaky}`);
});

// A non-flaky test reads its plan once at registration, so accessing t.assert
// before t.plan() must keep the historical counting (plan stays null, the later
// assertion is not counted, plan(0) is satisfied => PASS). The dynamic plan read
// added for flaky must not regress this; this guards that boundary.
test('flaky: non-flaky assert-before-plan counting is unchanged', async () => {
  const file = fixtures.path('test-runner/flaky/assert-before-plan.js');
  const events = await collectEvents(file);
  const name = 'assert accessed before plan (non-flaky)';
  const fails = byName(events.fail, name);
  assert.strictEqual(fails.length, 0, `non-flaky assert-before-plan must still pass, got ${fails.length} fail(s)`);
  const passes = byName(events.pass, name);
  assert.strictEqual(passes.length, 1);
  assert.strictEqual(passes[0].retryCount, undefined);
});

test('flaky: intermediate retries do not publish to the node.test error channel', async () => {
  const file = fixtures.path('test-runner/flaky/channel-silence.js');
  const { code, stderr, state: errors } = await spawnFlaky(file, 'chan-state.txt');
  assert.strictEqual(stderr, '');
  assert.strictEqual(code, 0);
  assert.strictEqual(errors, '0', `expected 0 channel errors for a passing flaky test, got ${errors}`);
});

// Flaky + subtests: a flaky test that creates a subtest on an early (failing)
// attempt and none on the final (passing) attempt used to crash the reporter -
// outputSubtestCount stayed > 0 while subtests was emptied on retry, so report()
// dereferenced an empty array and the run aborted with no summary. It must now
// complete normally.
//
// KNOWN LIMITATION: intermediate-attempt subtests are emitted to the reporter
// stream before the retry decision, so the output still contains leaked
// intermediate subtest LINES (their counts are parked and dropped on retry,
// keeping the summary and exit code correct). Removing the lines needs
// per-attempt event buffering.
test('flaky+subtest: a retried flaky test with subtests does not crash the run', async () => {
  const { code, stdout } = await tap(fixtures.path('test-runner/flaky/flaky-subtest.js'));
  assert.match(stdout, /# tests \d+/, 'a summary must be emitted (no crash)');
  assert.match(stdout, /^ok 1 - parent retries, subtest only on first attempt/m,
               'the flaky parent passes after retrying');
  assert.strictEqual(code, 0, `expected pass exit code, got ${code}`);
});

// Spawn a fixture under the TAP reporter with a FLAKY_STATE file it can write
// to, and return the captured output plus the file's final contents.
async function spawnFlaky(fixture, stateName) {
  const stateFile = stateName ? tmpdir.resolve(stateName) : undefined;
  const env = stateName ? { ...process.env, FLAKY_STATE: stateFile } : process.env;
  const { code, stdout, stderr } = await common.spawnPromisified(
    process.execPath, ['--test-reporter=tap', fixture], { env },
  );
  const state = stateFile ? require('node:fs').readFileSync(stateFile, 'utf8') : undefined;
  return { code, stdout, stderr, state };
}

test('flaky: expectFailure that unexpectedly passes is retried', async () => {
  // An unexpected pass (xpass) is a verdict failure, so a flaky expectFailure
  // test is retried like any other flaky failure. It never reaches the expected
  // failure, so it exhausts its retry budget and is reported as a failure;
  // retryCount === 5 confirms it was retried rather than run once.
  const file = fixtures.path('test-runner/flaky/expectfailure-unexpected-pass.js');
  const events = await collectEvents(file);
  const fails = byName(events.fail, 'flaky expectFailure unexpected pass');
  assert.strictEqual(fails.length, 1);
  assert.strictEqual(fails[0].retryCount, 5);
});

test('flaky: parent retries when only a subtest fails', async () => {
  // The failure surfaces only through the child, so the parent must keep
  // retrying until the child passes - and the final attempt's child must be
  // reported (reportOrder bookkeeping restarts with each retry).
  const events = await collectEvents(fixtures.path('test-runner/flaky/parent-subtest-retry.js'));
  const passes = byName(events.pass, 'parent retries when subtest fails');
  assert.strictEqual(passes.length, 1, `the flaky parent must ultimately pass, got ${passes.length}`);
  assert.strictEqual(passes[0].retryCount, 2,
                     `parent retried until the subtest passed; retryCount=${passes[0].retryCount}`);
  const childPasses = byName(events.pass, 'child');
  assert.strictEqual(childPasses.length, 1,
                     `the final attempt's child must be reported, got ${childPasses.length} pass(es)`);
});

test('flaky: tracing channel start/end stay balanced across retries', async () => {
  const { state } = await spawnFlaky(
    fixtures.path('test-runner/flaky/tracing-balance.js'), 'tracing.txt');
  const { 0: starts, 1: ends } = state.split(',');
  assert.strictEqual(starts, ends, `tracing start/end imbalance: starts=${starts} ends=${ends}`);
});

test('flaky: a flaky test does not propagate flakiness to its subtests', async () => {
  const { stdout } = await tap(fixtures.path('test-runner/flaky/subtest-no-inherit.js'));
  assert.match(stdout, /# flaky 1$/m);
  assert.doesNotMatch(stdout, /# flaky 2$/m);
});

test('flaky: MockTracker is reset between retries', async () => {
  const { code, stdout } = await tap(fixtures.path('test-runner/flaky/mock-reset.js'));
  assert.match(stdout, /^ok 1 - mock is reset between retries/m);
  assert.strictEqual(code, 0, `mock must be restored each retry, got exit ${code}`);
});

test('flaky: test-level after hook runs after the final attempt', async () => {
  const { code, state } = await spawnFlaky(
    fixtures.path('test-runner/flaky/after-final-attempt.js'), 'after-final.txt');
  assert.strictEqual(state, '3', `after ran at attempt ${state}; expected the final attempt (3)`);
  assert.strictEqual(code, 0);
});

test('flaky: a retried attempt aborts the previous attempt signal', async () => {
  const { code, stdout } = await tap(fixtures.path('test-runner/flaky/timeout-abort-prev.js'));
  assert.match(stdout, /^ok 1 - previous attempt signal is aborted before retry/m);
  assert.strictEqual(code, 0, `previous attempt signal must be aborted, got exit ${code}`);
});

test('flaky: in-flight subtest on a discarded attempt does not corrupt the run', async () => {
  const { code, stdout } = await tap(fixtures.path('test-runner/flaky/subtest-inflight.js'));
  assert.match(stdout, /# tests \d+/, 'a summary must be emitted (no crash/hang)');
  assert.strictEqual(code, 0, `the flaky parent must ultimately pass, got exit ${code}`);
});

test('flaky: expectFailure error is still published to the node.test channel', async () => {
  const { code, state } = await spawnFlaky(
    fixtures.path('test-runner/flaky/expectfailure-publish.js'), 'xf-publish.txt');
  assert.ok(Number(state) > 0, `error channel got ${state} publishes; expected > 0`);
  assert.strictEqual(code, 0, `expectFailure throw => test passes, got exit ${code}`);
});

test('flaky: intermediate subtest failure does not fail the run', async () => {
  // The discarded attempt's child failure must not reach the counters.
  const { code, stdout } = await tap(
    fixtures.path('test-runner/flaky/subtest-fail-then-pass.js'));
  assert.match(stdout, /# fail 0$/m);
  assert.match(stdout, /# cancelled 0$/m);
  assert.strictEqual(code, 0, `a flaky pass must exit 0, got ${code}`);
});

test('flaky: expectFailure timeout keeps non-flaky cancellation semantics', async () => {
  // It must not flow through fail(), where expectFailure would count the
  // timeout as a pass.
  const { code, stdout } = await tap(
    fixtures.path('test-runner/flaky/expectfailure-timeout.js'));
  assert.match(stdout, /# cancelled 1$/m);
  assert.match(stdout, /# pass 0$/m);
  assert.strictEqual(code, 1, `a timed-out expectFailure test must fail the run, got ${code}`);
});

test('flaky: after hook is not consumed between subtest-failure retries', async () => {
  // All three attempts' bodies must run first, teardowns last.
  const { code, state } = await spawnFlaky(
    fixtures.path('test-runner/flaky/after-subtest-retry.js'), 'after-subtest.txt');
  assert.match(state, /^body1;body2;body3;after1;/,
               `after must run only once all retries settle; got ${state}`);
  assert.strictEqual(code, 0);
});

test('flaky: hooks registered in the body do not accumulate across retries', async () => {
  const { code, state } = await spawnFlaky(
    fixtures.path('test-runner/flaky/hooks-in-body.js'), 'hooks-in-body.txt');
  assert.strictEqual(state, 'ae1;ae2;',
                     `each attempt's subtest must run only that attempt's afterEach; got ${state}`);
  assert.strictEqual(code, 0);
});

test('flaky: external abort after a retry stays cancelled', async () => {
  // Only an exhausted flaky timeout is upgraded to a failure.
  const { stdout } = await tap(fixtures.path('test-runner/flaky/abort-after-retry.js'));
  assert.match(stdout, /# cancelled 1$/m);
  assert.match(stdout, /# fail 0$/m);
});

test('flaky: child # flaky diagnostic does not leak under process isolation', async () => {
  // The parent filters per-child summary lines and emits one aggregate.
  const { code, stdout } = await common.spawnPromisified(
    process.execPath,
    ['--test', '--test-reporter=tap', fixtures.path('test-runner/flaky/eventually-passes.js')],
  );
  const flakyLines = stdout.match(/^# flaky /gm) ?? [];
  assert.strictEqual(flakyLines.length, 1,
                     `expected one aggregated flaky diagnostic, got ${flakyLines.length}`);
  assert.strictEqual(code, 0);
});

test('flaky: it.flaky keeps an explicit numeric budget', async () => {
  const { code, state, stdout } = await spawnFlaky(
    fixtures.path('test-runner/flaky/shorthand-budget.js'), 'shorthand-budget.txt');
  assert.strictEqual(state.length, 3,
                     `body ran ${state.length}x; flaky: 2 allows exactly 3 attempts`);
  assert.match(stdout, /# FLAKY failed after 2 re-tries/);
  assert.strictEqual(code, 1);
});

test('flaky: junit reporter emits the retry count as a raw number', async () => {
  const { stdout } = await common.spawnPromisified(
    process.execPath,
    ['--test-reporter=junit', fixtures.path('test-runner/flaky/eventually-passes.js')],
  );
  assert.match(stdout, /name="flaky" value="2"/);
});
