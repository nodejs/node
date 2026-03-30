'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/debugger');

const assert = require('assert');
const { createServer } = require('net');

// Launch w/ unavailable port.
(async () => {
  const blocker = createServer((socket) => socket.end());
  const port = await new Promise((resolve, reject) => {
    blocker.on('error', reject);
    blocker.listen(0, '127.0.0.1', () => resolve(blocker.address().port));
  });

  try {
    const script = fixtures.path('debugger', 'three-lines.js');
    const cli = startCLI([`--port=${port}`, script], [], {}, { randomPort: false });
    const code = await cli.quit();

    assert.doesNotMatch(
      cli.output,
      /report this bug/,
      'Omits message about reporting this as a bug');
    assert.ok(
      cli.output.includes(`waiting for 127.0.0.1:${port} to be free`),
      'Tells the user that the port wasn\'t available');
    assert.strictEqual(code, 1);
  } finally {
    blocker.close();
  }
})().then(common.mustCall());
