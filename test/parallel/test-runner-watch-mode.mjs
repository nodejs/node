// Flags: --expose-internals
import * as common from '../common/index.mjs';
import { describe, it } from 'node:test';
import { spawn } from 'node:child_process';
import { writeFileSync } from 'node:fs';
import util from 'internal/util';
import tmpdir from '../common/tmpdir.js';


if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

tmpdir.refresh();

// This test updates these files repeatedly,
// Reading them from disk is unreliable due to race conditions.
const fixtureContent = {
  'dependency.js': 'module.exports = {};',
  'dependency.mjs': 'export const a = 1;',
  'test.js': `
const test = require('node:test');
require('./dependency.js');
import('./dependency.mjs');
import('data:text/javascript,');
test('test has ran');`,
};
const fixturePaths = Object.keys(fixtureContent)
  .reduce((acc, file) => ({ ...acc, [file]: tmpdir.resolve(file) }), {});
Object.entries(fixtureContent)
  .forEach(([file, content]) => writeFileSync(fixturePaths[file], content));

async function testWatch({ fileToUpdate, file }) {
  const ran1 = util.createDeferredPromise();
  const ran2 = util.createDeferredPromise();
  const child = spawn(process.execPath,
                      ['--watch', '--test', file ? fixturePaths[file] : undefined].filter(Boolean),
                      { encoding: 'utf8', stdio: 'pipe', cwd: tmpdir.path });
  let stdout = '';

  child.stdout.on('data', (data) => {
    stdout += data.toString();
    const testRuns = stdout.match(/ - test has ran/g);
    if (testRuns?.length >= 1) ran1.resolve();
    if (testRuns?.length >= 2) ran2.resolve();
  });

  await ran1.promise;
  const content = fixtureContent[fileToUpdate];
  const path = fixturePaths[fileToUpdate];
  const interval = setInterval(() => writeFileSync(path, content), common.platformTimeout(1000));
  await ran2.promise;
  clearInterval(interval);
  child.kill();
}

describe('test runner watch mode', () => {
  it('should run tests repeatedly', async () => {
    await testWatch({ file: 'test.js', fileToUpdate: 'test.js' });
  });

  it('should run tests with dependency repeatedly', async () => {
    await testWatch({ file: 'test.js', fileToUpdate: 'dependency.js' });
  });

  it('should run tests with ESM dependency', async () => {
    await testWatch({ file: 'test.js', fileToUpdate: 'dependency.mjs' });
  });

  it('should support running tests without a file', async () => {
    await testWatch({ fileToUpdate: 'test.js' });
  });
});
