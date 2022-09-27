'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');
const path = require('path');

const scriptFullPath = fixtures.path('debugger', 'three-lines.js');
const script = path.relative(process.cwd(), scriptFullPath);

// Run after quit.
const runTest = async () => {
  const cli = startCLI([script]);
  try {
    await cli.waitForInitialBreak();
    await cli.waitForPrompt();
    await cli.command('breakpoints');
    assert.match(cli.output, /No breakpoints yet/);
    await cli.command('sb(2)');
    await cli.command('sb(3)');
    await cli.command('breakpoints');
    assert.ok(cli.output.includes(`#0 ${script}:2`));
    assert.ok(cli.output.includes(`#1 ${script}:3`));
    await cli.stepCommand('c'); // hit line 2
    await cli.stepCommand('c'); // hit line 3
    assert.deepStrictEqual(cli.breakInfo, { filename: script, line: 3 });
    await cli.command('restart');
    await cli.waitForInitialBreak();
    assert.deepStrictEqual(cli.breakInfo, { filename: script, line: 1 });
    await cli.stepCommand('c');
    assert.deepStrictEqual(cli.breakInfo, { filename: script, line: 2 });
    await cli.stepCommand('c');
    assert.deepStrictEqual(cli.breakInfo, { filename: script, line: 3 });
    await cli.command('breakpoints');
    const msg = `SCRIPT: ${script}, OUTPUT: ${cli.output}`;
    assert.ok(cli.output.includes(`#0 ${script}:2`), msg);
    assert.ok(cli.output.includes(`#1 ${script}:3`), msg);
  } finally {
    await cli.quit();
  }
};

runTest();
