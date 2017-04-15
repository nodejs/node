'use strict';
const { test } = require('tap');

const startCLI = require('./start-cli');

test('launch CLI w/o args', (t) => {
  const cli = startCLI([]);
  return cli.quit()
    .then((code) => {
      t.equal(code, 1, 'exits with non-zero exit code');
      t.match(cli.output, /^Usage:/, 'Prints usage info');
    });
});

test('launch w/ invalid host:port', (t) => {
  const cli = startCLI(['localhost:914']);
  return cli.quit()
    .then((code) => {
      t.match(
        cli.output,
        'failed to connect',
        'Tells the user that the connection failed');
      t.equal(code, 1, 'exits with non-zero exit code');
    });
});
