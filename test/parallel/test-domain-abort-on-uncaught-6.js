'use strict';

// This test makes sure that when using --abort-on-uncaught-exception and
// when throwing an error from within a domain that has an error handler
// setup, the process _does not_ abort.

const common = require('../common');
const assert = require('assert');
const domain = require('domain');
const net = require('net');

let errorHandlerCalled = false;

const test = () => {
  const d = domain.create();

  d.once('error', (err) => {
    errorHandlerCalled = true;
  });

  d.run(function() {
    const server = net.createServer((conn) => {
      conn.pipe(conn);
    });
    server.listen(0, common.localhostIPv4, () => {
      const conn = net.connect(this.address().port, common.localhostIPv4);
      conn.once('data', () => {
        throw new Error('ok');
      });
      conn.end('ok');
      server.close();
    });
  });
};


if (process.argv[2] === 'child') {
  test();
  process.on('exit', function onExit() {
    assert.strictEqual(errorHandlerCalled, true);
  });
} else {
  common.childShouldNotThrowAndAbort();
}
