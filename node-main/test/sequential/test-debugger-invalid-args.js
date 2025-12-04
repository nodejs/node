'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const startCLI = require('../common/debugger');

const assert = require('assert');

// Launch CLI w/o args.
(async () => {
  const cli = startCLI([], [], {}, { randomPort: false });
  const code = await cli.quit();
  assert.strictEqual(code, 9);
  assert.match(cli.output, /^Usage:/, 'Prints usage info');
})().then(common.mustCall());

// Launch w/ invalid host:port.
(async () => {
  const cli = startCLI([`localhost:${common.PORT}`], [], {}, { randomPort: false });
  const code = await cli.quit();
  assert.match(
    cli.output,
    /failed to connect/,
    'Tells the user that the connection failed');
  assert.strictEqual(code, 1);
})().then(common.mustCall());
