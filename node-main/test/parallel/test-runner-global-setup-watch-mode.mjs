import * as common from '../common/index.mjs';
import { beforeEach, describe, it } from 'node:test';
import { once } from 'node:events';
import assert from 'node:assert';
import { spawn } from 'node:child_process';
import { writeFileSync } from 'node:fs';
import tmpdir from '../common/tmpdir.js';

if (common.isIBMi)
  common.skip('IBMi does not support `fs.watch()`');

if (common.isAIX)
  common.skip('folder watch capability is limited in AIX.');

let fixturePaths;

// This test updates these files repeatedly,
// Reading them from disk is unreliable due to race conditions.
const fixtureContent = {
  'test.js': `
const test = require('node:test');
test('test with global hooks', (t) => {
  t.assert.ok('test passed');
});
`,
  'global-setup-teardown.js': `
async function globalSetup() {
  console.log('Global setup executed');
  process.on('message', (message) => {
    if (message === 'exit') {
      process.kill(process.pid, 'SIGINT');
    }
  });
}

async function globalTeardown() {
  console.log('Global teardown executed');
}

module.exports = { globalSetup, globalTeardown };  
`
};

function refresh() {
  tmpdir.refresh();
  fixturePaths = Object.keys(fixtureContent)
    .reduce((acc, file) => ({ ...acc, [file]: tmpdir.resolve(file) }), {});
  Object.entries(fixtureContent)
    .forEach(([file, content]) => writeFileSync(fixturePaths[file], content));
}

describe('test runner watch mode with global setup hooks', () => {
  beforeEach(refresh);
  for (const isolation of ['none', 'process']) {
    describe(`isolation: ${isolation}`, () => {
      it(`should run global setup/teardown hooks with each test run in watch mode`,
        // TODO(pmarchini): Skip test on Windows as the VS2022 build
        // has issues handling SIGTERM and SIGINT signals correctly.
        // See: https://github.com/nodejs/node/issues/46097
         { todo: common.isWindows },
         async () => {
           const globalSetupFileFixture = fixturePaths['global-setup-teardown.js'];
           const ran1 = Promise.withResolvers();
           const ran2 = Promise.withResolvers();

           const child = spawn(process.execPath,
                               [
                                 '--watch',
                                 '--test',
                                 '--test-reporter=spec',
                                 `--test-isolation=${isolation}`,
                                 '--test-global-setup=' + globalSetupFileFixture,
                                 fixturePaths['test.js'],
                               ],
                               {
                                 encoding: 'utf8',
                                 stdio: ['pipe', 'pipe', 'pipe', 'ipc'],
                                 cwd: tmpdir.path,
                               });

           let stdout = '';
           let currentRun = '';
           const runs = [];

           child.stdout.on('data', (data) => {
             stdout += data.toString();
             currentRun += data.toString();
             const testRuns = stdout.match(/duration_ms\s\d+/g);
             if (testRuns?.length >= 1) ran1.resolve();
             if (testRuns?.length >= 2) ran2.resolve();
           });

           await ran1.promise;
           runs.push(currentRun);
           currentRun = '';

           const content = fixtureContent['test.js'];
           const path = fixturePaths['test.js'];
           writeFileSync(path, content);

           await ran2.promise;
           runs.push(currentRun);

           currentRun = '';
           child.send('exit');
           await once(child, 'exit');

           assert.match(runs[0], /Global setup executed/);
           assert.match(runs[0], /tests 1/);
           assert.match(runs[0], /pass 1/);
           assert.match(runs[0], /fail 0/);

           assert.doesNotMatch(runs[1], /Global setup executed/);
           assert.doesNotMatch(runs[1], /Global teardown executed/);
           assert.match(runs[1], /tests 1/);
           assert.match(runs[1], /pass 1/);
           assert.match(runs[1], /fail 0/);

           // Verify stdout after killing the child
           assert.doesNotMatch(currentRun, /Global setup executed/);
           assert.match(currentRun, /Global teardown executed/);
         });
    });
  }
});
