'use strict';

const common = require('../common');
if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('node:assert');
const http2 = require('node:http2');
const debug = require('node:util').debuglog('test');

const testResBody = 'response content';

{
  // Invalid object value

  const server = http2.createServer();

  server.on('request', common.mustCall((req, res) => {
    debug('Server sending early hints...');
    res.writeEarlyHints('this should not be here');

    debug('Server sending full response...');
    res.end(testResBody);
  }));

  server.listen(0);

  server.on('listening', common.mustCall(() => {
    const client = http2.connect(`http://localhost:${server.address().port}`);
    const req = client.request();

    debug('Client sending request...');

    req.on('headers', common.mustNotCall());

    process.on('uncaughtException', (err) => {
      debug(`Caught an exception: ${JSON.stringify(err)}`);
      if (err.name === 'AssertionError') throw err;
      assert.strictEqual(err.code, 'ERR_INVALID_ARG_TYPE');
      process.exit(0);
    });
  }));
}
