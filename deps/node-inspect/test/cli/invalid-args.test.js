'use strict';
const Path = require('path');
const { createServer } = require('net');

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

test('launch w/ unavailable port', async (t) => {
  const blocker = createServer((socket) => socket.end());
  const port = await new Promise((resolve, reject) => {
    blocker.on('error', reject);
    blocker.listen(0, '127.0.0.1', () => resolve(blocker.address().port));
  });

  try {
    const script = Path.join('examples', 'three-lines.js');
    const cli = startCLI([`--port=${port}`, script]);
    const code = await cli.quit();

    t.notMatch(
      cli.output,
      'report this bug',
      'Omits message about reporting this as a bug');
    t.match(
      cli.output,
      `waiting for 127.0.0.1:${port} to be free`,
      'Tells the user that the port wasn\'t available');
    t.equal(code, 1, 'exits with non-zero exit code');
  } finally {
    blocker.close();
  }
});
