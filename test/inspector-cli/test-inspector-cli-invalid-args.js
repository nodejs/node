'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const startCLI = require('../common/inspector-cli');

const assert = require('assert');
const { createServer } = require('net');

// Launch CLI w/o args.
{
  const cli = startCLI([]);
  cli.quit()
    .then((code) => {
      assert.strictEqual(code, 1);
      assert.match(cli.output, /^Usage:/, 'Prints usage info');
    });
}

// Launch w/ invalid host:port.
{
  const cli = startCLI(['localhost:914']);
  cli.quit()
    .then((code) => {
      assert.match(
        cli.output,
        /failed to connect/,
        'Tells the user that the connection failed');
      assert.strictEqual(code, 1);
    });
}

// Launch w/ unavailable port.
(async () => {
  const blocker = createServer((socket) => socket.end());
  const port = await new Promise((resolve, reject) => {
    blocker.on('error', reject);
    blocker.listen(0, '127.0.0.1', () => resolve(blocker.address().port));
  });

  try {
    const script = fixtures.path('inspector-cli', 'three-lines.js');
    const cli = startCLI([`--port=${port}`, script]);
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
