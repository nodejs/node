// Flags: --no-warnings
import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import assert from 'node:assert';
import { spawnSync } from 'node:child_process';
import { test, run } from 'node:test';

const fixture = fixtures.path('test-runner', 'log-events', 'basic.mjs');

test('t.log() validates its arguments', (t) => {
  assert.throws(() => t.log(123), { code: 'ERR_INVALID_ARG_TYPE' });
});

test('built-in reporters render test:log like diagnostics', () => {
  const tap = spawnSync(process.execPath, ['--test', '--test-reporter=tap', fixture]);
  assert.match(tap.stdout.toString(), /# hello/);
  assert.match(tap.stdout.toString(), /# suite message/);

  const spec = spawnSync(process.execPath, ['--test', '--test-reporter=spec', fixture]);
  assert.match(spec.stdout.toString(), /ℹ hello/);
  assert.match(spec.stdout.toString(), /ℹ warned/);

  const junit = spawnSync(process.execPath, ['--test', '--test-reporter=junit', fixture]);
  assert.match(junit.stdout.toString(), /<!-- hello -->/);
});

test('test:log is emitted immediately with the expected payload', async () => {
  const stream = run({
    files: [fixture],
    isolation: 'none',
  });

  const logs = [];
  const events = [];

  stream.on('test:log', (data) => {
    logs.push(data);
    events.push(`log:${data.message}`);
  });

  stream.on('test:pass', (data) => {
    events.push(`pass:${data.name}`);
  });

  // eslint-disable-next-line no-unused-vars
  for await (const _ of stream);

  assert.strictEqual(logs.length, 3);

  const suiteLog = logs.find((log) => log.message === 'suite message');
  assert.strictEqual(suiteLog.name, 'my suite');
  assert.strictEqual(suiteLog.data, undefined);

  const hello = logs.find((log) => log.message === 'hello');
  assert.strictEqual(hello.name, 'logger');
  assert.ok(!('level' in hello));
  assert.deepStrictEqual(hello.data, { foo: 1 });
  assert.strictEqual(typeof hello.nesting, 'number');
  assert.strictEqual(typeof hello.testId, 'number');
  assert.strictEqual(typeof hello.parentId, 'number');
  assert.strictEqual(typeof hello.file, 'string');
  assert.strictEqual(typeof hello.line, 'number');
  assert.strictEqual(typeof hello.column, 'number');

  // The runner does not interpret data; conventions like a level live there.
  const warned = logs.find((log) => log.message === 'warned');
  assert.ok(!('level' in warned));
  assert.deepStrictEqual(warned.data, { level: 'warn', attempt: 2 });

  // 'logger' logs immediately while its earlier-declared sibling 'slow' is
  // still running, so the log must arrive before slow's buffered test:pass.
  const logIndex = events.indexOf('log:hello');
  const slowPassIndex = events.indexOf('pass:slow');
  assert.notStrictEqual(logIndex, -1);
  assert.notStrictEqual(slowPassIndex, -1);
  assert.ok(
    logIndex < slowPassIndex,
    `test:log should arrive before slow's buffered test:pass; events=${events.join(', ')}`,
  );
});
