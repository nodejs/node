'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const http2 = require('http2');
const { inspect } = require('util');

// Check if correct errors are emitted when wrong type of data is passed
// to certain options of ClientHttp2Session request method

const optionsToTest = {
  endStream: 'boolean',
  weight: 'number',
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

function determineSpecificType(value) {
  if (value == null) {
    return '' + value;
  }
  if (typeof value === 'function' && value.name) {
    return `function ${value.name}`;
  }
  if (typeof value === 'object') {
    if (value.constructor?.name) {
      return `an instance of ${value.constructor.name}`;
    }
    return `${inspect(value, { depth: -1 })}`;
  }
  let inspected = inspect(value, { colors: false });
  if (inspected.length > 28) { inspected = `${inspected.slice(0, 25)}...`; }

  return `type ${typeof value} (${inspected})`;
}

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
            message: `The "options.${option}" property must be of type ${optionsToTest[option]}. ` +
                    `Received ${determineSpecificType(types[type])}`
          });
      });
    });
    server.close();
    client.close();
  });
}));
