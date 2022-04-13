'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const startCLI = require('../common/debugger');

const assert = require('assert');

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
  const cli = startCLI([`localhost:${common.PORT}`]);
  cli.quit()
    .then((code) => {
      assert.match(
        cli.output,
        /failed to connect/,
        'Tells the user that the connection failed');
      assert.strictEqual(code, 1);
    });
}
