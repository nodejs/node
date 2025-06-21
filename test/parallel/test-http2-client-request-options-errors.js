'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');

// Check if correct errors are emitted when wrong type of data is passed
// to certain options of ClientHttp2Session request method

const optionsToTest = {
  endStream: 'boolean',
  parent: 'number',
  exclusive: 'boolean',
  silent: 'boolean'
};

const types = {
  boolean: true,
  function: () => {},
  number: 1,
  object: {},
  array: [],
  null: null,
  symbol: Symbol('test')
};

const server = http2.createServer(common.mustNotCall());

server.listen(0, common.mustCall(() => {
  const port = server.address().port;
  const client = http2.connect(`http://localhost:${port}`);

  client.on('connect', () => {
    Object.keys(optionsToTest).forEach((option) => {
      Object.keys(types).forEach((type) => {
        if (type === optionsToTest[option])
          return;

        assert.throws(
          () => client.request({
            ':method': 'CONNECT',
            ':authority': `localhost:${port}`
          }, {
            [option]: types[type]
          }), {
            name: 'TypeError',
            code: 'ERR_INVALID_ARG_TYPE',
          });
      });
    });
    server.close();
    client.close();
  });
}));
