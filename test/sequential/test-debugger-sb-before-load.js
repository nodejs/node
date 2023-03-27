'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');
const path = require('path');

// Using sb before loading file.

const scriptFullPath = fixtures.path('debugger', 'cjs', 'index.js');
const script = path.relative(process.cwd(), scriptFullPath);

const otherScriptFullPath = fixtures.path('debugger', 'cjs', 'other.js');
const otherScript = path.relative(process.cwd(), otherScriptFullPath);

const cli = startCLI(['--port=0', script]);

(async () => {
  await cli.waitForInitialBreak();
  await cli.waitForPrompt();
  await cli.command('sb("other.js", 2)');
  assert.match(cli.output, /not loaded yet/,
               'warns that the script was not loaded yet');
  await cli.stepCommand('cont');
  assert.ok(cli.output.includes(`break in ${otherScript}:2`),
            'found breakpoint in file that was not loaded yet');
})()
.then(common.mustCall())
.finally(() => cli.quit());
