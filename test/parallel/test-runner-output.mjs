import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import { describe, it } from 'node:test';

function replaceTestDuration(str) {
  return str
    .replaceAll(/duration_ms: 0(\r?\n)/g, 'duration_ms: ZERO$1')
    .replaceAll(/duration_ms: [0-9.]+/g, 'duration_ms: *')
    .replaceAll(/duration_ms [0-9.]+/g, 'duration_ms *');
}

const color = '(\\[\\d+m)';
const stackTraceBasePath = new RegExp(`${color}\\(${process.cwd()}/?${color}(.*)${color}\\)`, 'g');

function replaceSpecDuration(str) {
  return str
    .replaceAll(/\(0(\r?\n)ms\)/g, '(ZEROms)')
    .replaceAll(/[0-9.]+ms/g, '*ms')
    .replaceAll(/duration_ms [0-9.]+/g, 'duration_ms *')
    .replace(stackTraceBasePath, '$3');
}
const defaultTransform = snapshot
  .transform(snapshot.replaceWindowsLineEndings, snapshot.replaceStackTrace, replaceTestDuration);
const specTransform = snapshot
  .transform(replaceSpecDuration, snapshot.replaceWindowsLineEndings, snapshot.replaceStackTrace);


const tests = [
  { name: 'test-runner/output/abort.js' },
  { name: 'test-runner/output/abort_suite.js' },
  { name: 'test-runner/output/describe_it.js' },
  { name: 'test-runner/output/describe_nested.js' },
  { name: 'test-runner/output/hooks.js' },
  { name: 'test-runner/output/no_refs.js' },
  { name: 'test-runner/output/no_tests.js' },
  { name: 'test-runner/output/only_tests.js' },
  { name: 'test-runner/output/dot_reporter.js' },
  { name: 'test-runner/output/spec_reporter_successful.js', transform: specTransform },
  { name: 'test-runner/output/spec_reporter.js', transform: specTransform },
  { name: 'test-runner/output/spec_reporter_cli.js', transform: specTransform },
  { name: 'test-runner/output/output.js' },
  { name: 'test-runner/output/output_cli.js' },
  { name: 'test-runner/output/name_pattern.js' },
  { name: 'test-runner/output/name_pattern_with_only.js' },
  { name: 'test-runner/output/unresolved_promise.js' },
  { name: 'test-runner/output/default_output.js', transform: specTransform, tty: true },
].map(({ name, tty, transform }) => ({
  name,
  fn: common.mustCall(async () => {
    await snapshot.spawnAndAssert(fixtures.path(name), transform ?? defaultTransform, { tty });
  }),
}));

describe('test runner output', { concurrency: true }, () => {
  for (const { name, fn } of tests) {
    it(name, fn);
  }
});
