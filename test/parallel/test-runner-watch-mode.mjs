// Flags: --expose-internals
import '../common/index.mjs';
import { describe, it } from 'node:test';
import { spawn } from 'node:child_process';
import { writeFileSync } from 'node:fs';
import util from 'internal/util';
import * as fixtures from '../common/fixtures.mjs';

// This test updates these files repeatedly,
// Reading them from disk is unreliable due to race conditions.
const fixtureContent = {
  [fixtures.path('test-runner/dependent.js')]:
`const test = require('node:test');
require('./dependency.js');
import('./dependency.mjs');
import('data:text/javascript,');
test('test has ran');
`,
  [fixtures.path('test-runner/dependency.js')]:
`module.exports = {};
`,
  [fixtures.path('test-runner/dependency.mjs')]:
`export const a = 1;
`,
};

async function testWatch({ files, fileToUpdate }) {
  const ran1 = util.createDeferredPromise();
  const ran2 = util.createDeferredPromise();
  const child = spawn(process.execPath, ['--watch', '--test', '--no-warnings', ...files], { encoding: 'utf8' });
  let stdout = '';

  child.stdout.on('data', (data) => {
    stdout += data.toString();
    const testRuns = stdout.match(/ - test has ran/g);
    if (testRuns?.length >= 1) ran1.resolve();
    if (testRuns?.length >= 2) ran2.resolve();
  });

  await ran1.promise;
  const content = fixtureContent[fileToUpdate];
  const interval = setInterval(() => writeFileSync(fileToUpdate, content), 10);
  await ran2.promise;
  clearInterval(interval);
  child.kill();
}

describe('test runner watch mode', () => {
  it('should run tests repeatedly', async () => {
    const file1 = fixtures.path('test-runner/index.test.js');
    const file2 = fixtures.path('test-runner/dependent.js');
    await testWatch({ files: [file1, file2], fileToUpdate: file2 });
  });

  it('should run tests with dependency repeatedly', async () => {
    const file1 = fixtures.path('test-runner/index.test.js');
    const dependent = fixtures.path('test-runner/dependent.js');
    const dependency = fixtures.path('test-runner/dependency.js');
    await testWatch({ files: [file1, dependent], fileToUpdate: dependency });
  });

  it('should run tests with ESM dependency', async () => {
    const file1 = fixtures.path('test-runner/index.test.js');
    const dependent = fixtures.path('test-runner/dependent.js');
    const dependency = fixtures.path('test-runner/dependency.mjs');
    await testWatch({ files: [file1, dependent], fileToUpdate: dependency });
  });
});
