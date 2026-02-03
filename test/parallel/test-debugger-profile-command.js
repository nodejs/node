'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');
const fs = require('fs');
const path = require('path');

const cli = startCLI([fixtures.path('debugger/empty.js')]);

const rootDir = path.resolve(__dirname, '..', '..');

(async () => {
  await cli.waitForInitialBreak();
  await cli.waitForPrompt();
  await cli.command('profile');
  await cli.command('profileEnd');
  assert.match(cli.output, /\[Profile \d+μs\]/);
  await cli.command('profiles');
  assert.match(cli.output, /\[ \[Profile \d+μs\] \]/);
  await cli.command('profiles[0].save()');
  assert.match(cli.output, /Saved profile to .*node\.cpuprofile/);

  const cpuprofile = path.resolve(rootDir, 'node.cpuprofile');
  const data = JSON.parse(fs.readFileSync(cpuprofile, 'utf8'));
  assert.strictEqual(Array.isArray(data.nodes), true);

  fs.rmSync(cpuprofile);
})()
.then(common.mustCall())
.finally(() => cli.quit());
