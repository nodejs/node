'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();
const assert = require('assert');
const fixtures = require('../common/fixtures');
const spawn = require('child_process').spawn;

const proc = spawn(process.execPath, [
  'inspect', fixtures.path('debugger', 'alive.js'),
]);
proc.stdout.setEncoding('utf8');

const TIMEOUT = common.platformTimeout(10_000);
let output = '';
let promptSeen = false;

(async () => {
  await new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      proc.kill();
      reject(new Error(`Timed out waiting for the debugger prompt; output: ${output}`));
    }, TIMEOUT);

    proc.stdout.on('data', (data) => {
      output += data;
      if (output.includes('debug> ') && !promptSeen) {
        promptSeen = true;
        proc.stdin.end('.exit\n');
      }
    });

    proc.once('error', reject);
    proc.once('close', common.mustCall((code, signal) => {
      clearTimeout(timer);
      if (!promptSeen) {
        reject(new Error(
          `Debugger exited before showing the prompt (code ${code}, signal ${signal}); ` +
          `output: ${output}`));
        return;
      }
      assert.strictEqual(code, 0);
      resolve();
    }));
  });
})().then(common.mustCall());
