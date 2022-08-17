'use strict';
const common = require('../common');
const assert = require('node:assert');
const http = require('node:http');
const debug = require('node:util').debuglog('test');

const testResBody = 'response content\n';

const server = http.createServer(common.mustCall((req, res) => {
  debug('Server sending early hints...');
  res.writeEarlyHints({ links: 'bad argument object' });

  debug('Server sending full response...');
  res.end(testResBody);
}));

server.listen(0, common.mustCall(() => {
  const req = http.request({
    port: server.address().port, path: '/'
  });

  req.end();
  debug('Client sending request...');

  req.on('information', common.mustNotCall());

  process.on('uncaughtException', (err) => {
    debug(`Caught an exception: ${JSON.stringify(err)}`);
    if (err.name === 'AssertionError') throw err;
    assert.strictEqual(err.code, 'ERR_INVALID_ARG_VALUE');
    process.exit(0);
  });
}));
