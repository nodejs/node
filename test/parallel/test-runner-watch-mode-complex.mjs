// Flags: --expose-internals
import * as common from '../common/index.mjs';
import { describe, it } from 'node:test';
import assert from 'node:assert';
import { spawn } from 'node:child_process';
import { writeFileSync, unlinkSync } from 'node:fs';
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
  // Test 1
  'test.js': `
const test = require('node:test');
require('./dependency.js');
import('./dependency.mjs');
import('data:text/javascript,');
test('first test has ran');`,
  // Test 2
  'test-2.mjs': `
import test from 'node:test';
import './dependency.js';
import { a } from './dependency.mjs';
import 'data:text/javascript,';
test('second test has ran');`,
  // Test file to be deleted
  'test-to-delete.mjs': `
import test from 'node:test';
import './dependency.js';
import { a } from './dependency.mjs';
import 'data:text/javascript,';
test('test to delete has ran');`,
};

const fixturePaths = Object.keys(fixtureContent)
  .reduce((acc, file) => ({ ...acc, [file]: tmpdir.resolve(file) }), {});
Object.entries(fixtureContent)
  .forEach(([file, content]) => writeFileSync(fixturePaths[file], content));

describe('test runner watch mode with more complex setup', () => {
  it('should run tests when a dependency changed after a watched test file being deleted', async () => {
    const ran1 = util.createDeferredPromise();
    const ran2 = util.createDeferredPromise();
    const child = spawn(process.execPath,
                        ['--watch', '--test'].filter(Boolean),
                        { encoding: 'utf8', stdio: 'pipe', cwd: tmpdir.path });
    let stdout = '';
    let currentRun = '';
    const runs = [];

    child.stdout.on('data', (data) => {
      stdout += data.toString();
      currentRun += data.toString();
      const testRuns = stdout.match(/# duration_ms\s\d+/g);

      if (testRuns?.length >= 1) ran1.resolve();
      if (testRuns?.length >= 2) ran2.resolve();
    });

    await ran1.promise;
    runs.push(currentRun);
    currentRun = '';
    const fileToDeletePathLocal = tmpdir.resolve('test-to-delete.mjs');
    unlinkSync(fileToDeletePathLocal);

    const content = fixtureContent['dependency.mjs'];
    const path = fixturePaths['dependency.mjs'];
    const interval = setInterval(() => writeFileSync(path, content), common.platformTimeout(1000));
    await ran2.promise;
    runs.push(currentRun);
    currentRun = '';
    clearInterval(interval);
    child.kill();

    assert.strictEqual(runs.length, 2);

    for (let i = 0; i < runs.length; i++) {
      if (i === 0) {
        assert.match(runs[i], /# tests 3/);
        assert.match(runs[i], /# pass 3/);
        assert.match(runs[i], /# fail 0/);
        assert.match(runs[i], /# cancelled 0/);
      } else {
        assert.match(runs[i], /# tests 2/);
        assert.match(runs[i], /# pass 2/);
        assert.match(runs[i], /# fail 0/);
        assert.match(runs[i], /# cancelled 0/);
      }
    }
  });
});
