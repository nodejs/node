'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');
const { spawn } = require('child_process');
const { once } = require('events');

const script = fixtures.path('debugger', 'alive.js');

(async () => {
  const target = spawn(process.execPath, [script], {
    stdio: ['ignore', 'pipe', 'pipe', 'ipc'],
  });
  await once(target, 'message');
  const cli = startCLI(['-p', `${target.pid}`], [], {}, { randomPort: false });

  try {
    await cli.waitForPrompt();
    await cli.command('sb("alive.js", 3)');
    await cli.waitFor(/break/);
    await cli.waitForPrompt();
    assert.match(
      cli.output,
      /> 3 {3}\+\+x;/,
      'marks the 3rd line');
  } finally {
    const targetClosed = once(target, 'close');
    target.kill();
    await Promise.all([cli.quit(), targetClosed]);
  }
})().then(common.mustCall());
