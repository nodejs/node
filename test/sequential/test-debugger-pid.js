'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');
const net = require('net');
const { spawn } = require('child_process');

const script = fixtures.path('debugger', 'alive.js');

// The -p <pid> attach mode uses the default inspector port (9229).
// If that port is already in use, the test will hang. Check first and skip.
function checkPort(port) {
  const { promise, resolve, reject } = Promise.withResolvers();
  const server = net.createServer();
  server.once('error', reject);
  server.listen(port, '127.0.0.1', () => server.close(resolve));
  return promise;
}

(async () => {
  try {
    await checkPort(9229);
  } catch (e) {
    // In the typical case, this test runs in sequential set and depends on
    // nothing else bound to port 9229. However, if there's some other failure
    // that causes an orphaned process to be left running on port 9229, this
    // test was hanging and timing out. Let's arrange to skip instead of hanging.
    if (e.code === 'EADDRINUSE') {
      common.skip('Port 9229 is already in use');
    }
    throw e;
  }

  const target = spawn(process.execPath, [script]);
  const cli = startCLI(['-p', `${target.pid}`], [], {}, { randomPort: false });

  try {
    await cli.waitForPrompt();
    await cli.command('sb("alive.js", 3)');
    await cli.waitFor(/break/);
    await cli.waitForPrompt();
    assert.match(cli.output, /> 3 {3}\+\+x;/, 'marks the 3rd line');
  } finally {
    target.kill();
    await Promise.race([
      cli.quit(),
      new Promise((resolve) => setTimeout(resolve, 5000)),
    ]);
  }
})().then(common.mustCall());
