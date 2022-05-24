'use strict';
require('../common');
const assert = require('assert');
const net = require('net');

{
  const invalidKeys = [
    'objectMode',
    'readableObjectMode',
    'writableObjectMode',
  ];
  invalidKeys.forEach((invalidKey) => {
    const option = {
      port: 8080,
      [invalidKey]: true
    };

    assert.throws(() => {
      net.createConnection(option);
    }, {
      code: 'ERR_INVALID_ARG_VALUE',
      name: 'TypeError',
    });
  });
}
