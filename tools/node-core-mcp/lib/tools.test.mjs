import { test } from 'node:test';
import assert from 'node:assert/strict';
import { fileURLToPath } from 'node:url';
import { dirname, resolve } from 'node:path';
import { registerTools, inferSubsystemFromPath, truncate } from './tools.mjs';

const REPO_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..', '..', '..');

function createMock() {
  const handlers = new Map();
  const mock = {
    tool(name, _desc, _schema, handler) { handlers.set(name, handler); },
    async call(name, args = {}) {
      const h = handlers.get(name);
      if (!h) throw new Error(`No tool registered: ${name}`);
      return h(args);
    },
  };
  registerTools(mock, REPO_ROOT);
  return mock;
}

// ── inferSubsystemFromPath ────────────────────────────────────────────────

test('inferSubsystemFromPath: lib/internal/url.js → url', () => {
  assert.strictEqual(inferSubsystemFromPath('lib/internal/url.js'), 'url');
});

test('inferSubsystemFromPath: lib/fs.js → fs', () => {
  assert.strictEqual(inferSubsystemFromPath('lib/fs.js'), 'fs');
});

test('inferSubsystemFromPath: src/node_url.cc → url', () => {
  assert.strictEqual(inferSubsystemFromPath('src/node_url.cc'), 'url');
});

test('inferSubsystemFromPath: test/parallel/test-whatwg-url-foo.js → whatwg', () => {
  assert.strictEqual(inferSubsystemFromPath('test/parallel/test-whatwg-url-foo.js'), 'whatwg');
});

test('inferSubsystemFromPath: doc/api/stream.md → stream', () => {
  assert.strictEqual(inferSubsystemFromPath('doc/api/stream.md'), 'stream');
});

test('inferSubsystemFromPath: tools/foo.js → tools', () => {
  assert.strictEqual(inferSubsystemFromPath('tools/foo.js'), 'tools');
});

// ── truncate ─────────────────────────────────────────────────────────────

test('truncate: short string passes through unchanged', () => {
  assert.strictEqual(truncate('hello'), 'hello');
});

test('truncate: long string is cut at limit with annotation', () => {
  const s = 'x'.repeat(100);
  const result = truncate(s, 10);
  assert.strictEqual(result.length > 10, true);
  assert.ok(result.includes('[truncated'));
  assert.ok(result.startsWith('x'.repeat(10)));
});

// ── explain_test_failure ─────────────────────────────────────────────────

test('explain_test_failure: parses TAP not-ok line', async () => {
  const mock = createMock();
  const log = [
    'TAP version 13',
    '1..2',
    'not ok 1 - test/parallel/test-foo.js',
    '# AssertionError: expected 1 to equal 2',
    'ok 2 - test/parallel/test-bar.js',
  ].join('\n');
  const res = await mock.call('explain_test_failure', { log });
  const data = JSON.parse(res.content[0].text);
  assert.strictEqual(data.failureCount, 1);
  assert.strictEqual(data.failures[0].test, 'test/parallel/test-foo.js');
  assert.ok(data.failures[0].details.includes('AssertionError'));
  assert.ok(data.rerunCommands[0].includes('test/parallel/test-foo.js'));
});

test('explain_test_failure: parses tools/test.py FAIL line', async () => {
  const mock = createMock();
  const log = 'FAIL test/parallel/test-baz.js\n';
  const res = await mock.call('explain_test_failure', { log });
  const data = JSON.parse(res.content[0].text);
  assert.strictEqual(data.failureCount, 1);
  assert.strictEqual(data.failures[0].test, 'test/parallel/test-baz.js');
});

test('explain_test_failure: reports no failures for passing log', async () => {
  const mock = createMock();
  const res = await mock.call('explain_test_failure', { log: 'ok 1 - test-foo\nok 2 - test-bar\n' });
  assert.strictEqual(res.content[0].text, 'No test failures detected in the provided log.');
});

// ── find_subsystem ────────────────────────────────────────────────────────

test('find_subsystem: url files return url subsystem', async () => {
  const mock = createMock();
  const res = await mock.call('find_subsystem', {
    files: ['lib/internal/url.js', 'test/parallel/test-whatwg-url-custom-searchparams.js'],
  });
  const data = JSON.parse(res.content[0].text);
  assert.ok(data.subsystem, 'missing subsystem');
  assert.ok(data.labels.includes('test'));
  assert.ok(Array.isArray(data.likelyReviewers));
});

// ── list_relevant_tests ───────────────────────────────────────────────────

test('list_relevant_tests: returns commands array and reason string', async () => {
  const mock = createMock();
  const res = await mock.call('list_relevant_tests', { changedFiles: ['lib/internal/url.js'] });
  const data = JSON.parse(res.content[0].text);
  assert.ok(Array.isArray(data.commands));
  assert.ok(typeof data.reason === 'string');
});

// ── list_docs ─────────────────────────────────────────────────────────────

test('list_docs: returns .md file names', async () => {
  const mock = createMock();
  const res = await mock.call('list_docs', {});
  assert.ok(res.content[0].text.includes('.md'));
});
