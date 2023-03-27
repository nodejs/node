'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');

const cli = startCLI(['--port=0', fixtures.path('debugger/break.js')]);

(async () => {
  await cli.waitForInitialBreak();
  await cli.command('watch()');
  await cli.waitFor(/ERR_INVALID_ARG_TYPE/);
  assert.match(cli.output, /TypeError \[ERR_INVALID_ARG_TYPE\]: The "expression" argument must be of type string\. Received undefined/);
})()
.finally(() => cli.quit())
.then(common.mustCall());
