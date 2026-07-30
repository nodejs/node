'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');
const tmpdir = require('../common/tmpdir');

const assert = require('assert');
const fs = require('fs');

tmpdir.refresh();

const cli = startCLI(
  [fixtures.path('debugger/empty.js')],
  [],
  { cwd: tmpdir.path },
);

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

  const cpuprofile = tmpdir.resolve('node.cpuprofile');
  const data = JSON.parse(fs.readFileSync(cpuprofile, 'utf8'));
  assert.strictEqual(Array.isArray(data.nodes), true);

  fs.rmSync(cpuprofile);
})()
.then(common.mustCall())
.finally(() => cli.quit());
