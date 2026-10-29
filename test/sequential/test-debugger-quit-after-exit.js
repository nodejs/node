'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('assert');
const startCLI = require('../common/debugger');

(async () => {
  const cli = startCLI(['--help'], [], {}, { randomPort: false });
  await assert.rejects(cli.waitForPrompt(), /Child exited while waiting/);
  await cli.quit();
})().then(common.mustCall());
