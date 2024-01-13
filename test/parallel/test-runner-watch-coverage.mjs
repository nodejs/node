// Flags: --expose-internals
import * as common from '../common/index.mjs';
import { test } from 'node:test';
import { spawn } from 'node:child_process';
import { writeFileSync } from 'node:fs';
import util from 'internal/util';
import tmpdir from '../common/tmpdir.js';
import assert from 'node:assert';

const skipIfNoInspector = {
  skip: !process.features.inspector ? 'inspector disabled' : false
};

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

function getCoverageFixtureReport() {
  const report = [
    '# start of coverage report',
    '# ---------------------------------------------------------------',
    '# file           | line % | branch % | funcs % | uncovered lines',
    '# ---------------------------------------------------------------',
    '# dependency.js  | 100.00 |   100.00 |  100.00 | ',
    '# dependency.mjs | 100.00 |   100.00 |  100.00 | ',
    '# test.js        | 100.00 |   100.00 |  100.00 | ',
    '# ---------------------------------------------------------------',
    '# all files      | 100.00 |   100.00 |  100.00 |',
    '# ---------------------------------------------------------------',
    '# end of coverage report',
  ].join('\n');

  if (common.isWindows) {
    return report.replaceAll('/', '\\');
  }

  return report;
}

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

tmpdir.refresh();

const fixturePaths = Object.keys(fixtureContent)
  .reduce((acc, file) => ({ ...acc, [file]: tmpdir.resolve(file) }), {});
Object.entries(fixtureContent)
  .forEach(([file, content]) => writeFileSync(fixturePaths[file], content));

async function testWatch({ fileToUpdate, file }) {
  const ran1 = util.createDeferredPromise();
  const ran2 = util.createDeferredPromise();
  const args = [ '--test', '--watch', '--experimental-test-coverage',
                 '--test-reporter', 'tap', file ? fixturePaths[file] : undefined];
  const child = spawn(process.execPath,
                      args.filter(Boolean),
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

  return stdout;
}

test('should report coverage report with watch mode', skipIfNoInspector, async () => {
  const stdout = await testWatch({ file: 'test.js', fileToUpdate: 'test.js' });
  const expectedCoverageReport = getCoverageFixtureReport();
  assert(stdout.includes(expectedCoverageReport));
});
